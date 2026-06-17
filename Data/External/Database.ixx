export module GW2Viewer.Data.External.Database;
import GW2Viewer.Common;
import GW2Viewer.Common.GUID;
import GW2Viewer.Common.Time;
import GW2Viewer.Utils.Async.ProgressBarContext;
import GW2Viewer.Utils.Enum;
import GW2Viewer.Utils.Thread;
import std;
import sqlite_modern_cpp;
import <concurrentqueue/blockingconcurrentqueue.h>;

namespace GW2Viewer::Data::External
{

template<typename T> struct CoerceArgumentType { using Type = T; };
template<> struct CoerceArgumentType<uint64> { using Type = sqlite_uint64; };
template<> struct CoerceArgumentType<int64> { using Type = sqlite_int64; };
template<> struct CoerceArgumentType<Degrees> { using Type = float; };
template<> struct CoerceArgumentType<Radians> { using Type = float; };
template<> struct CoerceArgumentType<GUID> { using Type = std::vector<byte>; };
template<Enumeration Enum> struct CoerceArgumentType<Enum> { using Type = std::underlying_type_t<Enum>; };

struct OperationCancelledException { };
struct OperationOptions
{
    std::string Joins = "";
    size_t Limit = 0;
    std::shared_mutex* SharedMutex = nullptr;
    std::function<void()> PostHandler = nullptr;
};
struct OperationBase
{
    std::string Table;
    OperationOptions Options;
    bool volatile Cancelled = false;
    bool Running = false;

    OperationBase(std::string_view table, OperationOptions&& options) : Table(table), Options(std::move(options)) { }
    virtual ~OperationBase() = default;
    void Process(sqlite::database& db, Utils::Async::ProgressBarContext* progress, Time::Duration busyTimeout)
    {
        Running = true;
        if (Options.SharedMutex)
            Options.SharedMutex->lock();

        sqlite3_progress_handler(db.connection().get(), 1000, [](void* operation) { return +((OperationBase*)operation)->Cancelled; }, this);
        while (!Cancelled)
        {
            try { ProcessImpl(db, progress); break; }
            catch (sqlite::errors::busy const&) { Utils::Thread::Sleep(busyTimeout); }
            catch (sqlite::errors::interrupt const&) { break; }
            catch (OperationCancelledException const&) { break; }
        }
        sqlite3_progress_handler(db.connection().get(), 0, nullptr, nullptr);

        if (Options.SharedMutex)
            Options.SharedMutex->unlock();

        PostProcessImpl();
        Running = false;
    }

protected:
    virtual void ProcessImpl(sqlite::database& db, Utils::Async::ProgressBarContext* progress) = 0;
    virtual void PostProcessImpl();
};

template<typename... Args>
struct QueryOperation : OperationBase
{
    std::string Columns;
    std::string Condition;
    std::function<void(Args...)> Handler;
    std::string Query = std::format("SELECT {} FROM {} {} WHERE {}{}", Columns, Table, Options.Joins, Condition, Options.Limit ? std::format(" LIMIT {}", Options.Limit) : "");
    size_t LoadedRows = 0;

    QueryOperation(std::string_view table, std::string_view columns, std::string_view condition, std::function<void(Args...)>&& handler, OperationOptions&& options) : OperationBase(table, std::move(options)), Columns(columns), Condition(condition), Handler(std::move(handler)) { }

    void ProcessImpl(sqlite::database& db, Utils::Async::ProgressBarContext* progress) override
    {
        db << Query >> [this, progress, processed = 0, nextUpdate = Time::FrameStart](typename CoerceArgumentType<std::decay_t<Args>>::Type... args) mutable
        {
            if (Cancelled)
                throw OperationCancelledException();

            Handler(((Args)args)...);
            ++LoadedRows;
            if (progress && Time::FrameStart >= nextUpdate)
            {
                *progress = LoadedRows;
                nextUpdate = Time::FrameStart + 50ms;
            }
        };
        if (progress)
            progress->Start(LoadedRows, LoadedRows);
    }
};

template<typename... Args>
struct LoadingOperation : QueryOperation<Args...>
{
    using Base = QueryOperation<Args...>;

    std::string QueryMax = std::format("SELECT MAX({}._rowid_) FROM {}", Base::Table, Base::Table);
    sqlite_int64 MaxRowID = -1;
    sqlite_int64 CurrentRowID = -1;
    sqlite_int64 LastCurrentRowID = -1;

    LoadingOperation(std::string_view table, std::string_view columns, std::function<void(Args...)>&& handler, OperationOptions&& options) : Base(table, std::format("{}._rowid_, {}", table, columns), std::format("{}._rowid_ > ?", table), std::move(handler), std::move(options)) { }

    void ProcessImpl(sqlite::database& db, Utils::Async::ProgressBarContext* progress) override
    {
        if (progress)
        {
            db << QueryMax >> MaxRowID;
            progress->Start(Base::Table, MaxRowID);
        }

        db << Base::Query << CurrentRowID >> [this, progress, processed = 0, interval = std::max<uint32>(1, MaxRowID / 1000), nextUpdate = Time::FrameStart](sqlite_int64 rowID, typename CoerceArgumentType<std::decay_t<Args>>::Type... args) mutable
        {
            if (Base::Cancelled)
                throw OperationCancelledException();

            Base::Handler(((Args)args)...);
            CurrentRowID = std::max(CurrentRowID, rowID);
            if (progress)
            {
                if (!(++processed % interval) || Time::FrameStart >= nextUpdate)
                {
                    *progress = CurrentRowID;
                    nextUpdate = Time::FrameStart + 50ms;
                }
            }
        };
        if (progress)
            *progress = MaxRowID;
    }
    void PostProcessImpl() override
    {
        if (LastCurrentRowID != CurrentRowID)
        {
            LastCurrentRowID = CurrentRowID;
            OperationBase::PostProcessImpl();
        }
    }
};

export
{

// Calling QueryToken::Cancel() or destroying QueryToken will issue a cancel request to the
// underlying query and block until the query finishes, but will not interrupt any ongoing
// callbacks. When storing QueryToken inside a class, prefer to place them at the end, so they
// get destroyed first, before destroying any other resources that might be used by the callback.
struct QueryToken
{
    QueryToken() { }
    QueryToken(std::shared_ptr<OperationBase> const& operation) : m_operation(operation) { }
    QueryToken(QueryToken const&) = delete;
    QueryToken(QueryToken&&) = delete;
    ~QueryToken() { Cancel(); }

    QueryToken& operator=(QueryToken const&) = delete;
    QueryToken& operator=(QueryToken&& source) noexcept
    {
        m_operation = std::move(source.m_operation);
        m_cancelled = std::exchange(source.m_cancelled, true);
        return *this;
    }

    void Cancel()
    {
        if (m_cancelled)
            return;
        m_cancelled = true;

        if (auto operation = m_operation.lock())
        {
            operation->Cancelled = true;
            Utils::Thread::SleepWhile(10ms, [&] { return operation->Running; });
        }
    }

private:
    std::weak_ptr<OperationBase> m_operation;
    bool m_cancelled = false;
};

class Database
{
    static constexpr size_t CONCURRENT_COMMAND_WORKERS = 5;

public:
    QueryToken Query(std::string_view table, std::string_view columns, std::string_view condition, auto&& handler, OperationOptions&& options)
    {
        return { AddCommand(std::shared_ptr<OperationBase>(new QueryOperation(table, columns, condition, std::function(handler), std::move(options)))) };
    }
    void Load(std::string_view table, std::string_view columns, auto&& handler, OperationOptions&& options)
    {
        Loading.Operations.emplace_back(new LoadingOperation(table, columns, std::function(handler), std::move(options)));
    }

    void Load(std::filesystem::path const& path, Utils::Async::ProgressBarContext& progress)
    {
        CreateLoadOperations();
        for (auto& connection : Command)
            connection.Start(path, progress);
        Loading.Start(path, progress);
    }

private:
    struct Connection
    {
        std::optional<sqlite::database> Database;
        std::jthread Thread;
        bool ExitRequested = false;

        void Start(std::filesystem::path const& path, Utils::Async::ProgressBarContext& progress)
        {
            Connect(path);
            OnLoaded(progress);
            StartLoop();
        }
        virtual ~Connection() { ExitRequested = true; }

    protected:
        void Connect(std::filesystem::path const& path)
        {
            Database = sqlite::database { path.u16string(), { .flags = sqlite::OpenFlags::READONLY } };
        }
        void StartLoop()
        {
            Thread = std::jthread([this]
            {
                while (!ExitRequested)
                    OnLoop();
            });
        }

        virtual void OnLoaded(Utils::Async::ProgressBarContext& progress) { }
        virtual void OnLoop() = 0;
    };

    struct CommandConnection : Connection
    {
        void OnLoop() override;
    } Command[CONCURRENT_COMMAND_WORKERS];
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<OperationBase>> CommandQueue;
    std::shared_ptr<OperationBase> AddCommand(std::shared_ptr<OperationBase> command);

    struct : Connection
    {
        std::list<std::unique_ptr<OperationBase>> Operations;

        void OnLoaded(Utils::Async::ProgressBarContext& progress) override
        {
            progress.Start("Loading external DB", Operations.size());
            for (auto&& operation : Operations)
            {
                operation->Process(*Database, &progress.GetChild(), 1s);
                ++progress;
            }
        }
        void OnLoop() override
        {
            for (auto&& operation : Operations)
                operation->Process(*Database, nullptr, 1s);

            Utils::Thread::Sleep(1s);
        }
    } Loading;
    void CreateLoadOperations();
};

}

}

export namespace GW2Viewer::G { Data::External::Database Database; }
