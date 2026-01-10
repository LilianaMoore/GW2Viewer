module;
#include <imgui_internal.h>
#include "dep/imguiwrap.dear.h"

export module GW2Viewer.UI.ImGui:Wrap;
import :Core;
import std;

namespace dear
{

template<typename Base>
struct TableDockBase : ScopeWrapper<Base, true>
{
    TableDockBase(char const* str_id, int columns, ImVec2 size) : ScopeWrapper<Base, true>((
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2()),
        ImGui::BeginTable(str_id, columns, ImGuiTableFlags_NoSavedSettings, size)))
    {
        if (this->ok_)
            static_cast<Base*>(this)->ctor();
    }
    void dtor() const noexcept
    {
        if (this->ok_)
            ImGui::EndTable();
        ImGui::PopStyleVar(1);
    }
};

export
{

using Window = Begin;
using dear::Child;
using dear::Group;
using dear::Combo;
using dear::ListBox;
using dear::MenuBar;
using dear::MainMenuBar;
using dear::Menu;
using dear::Table;
using dear::Tooltip;
using dear::CollapsingHeader;
using dear::TreeNode;
using dear::SeparatedTreeNode;
using dear::TreeNodeEx;
using dear::SeparatedTreeNodeEx;
using dear::Popup;
using dear::PopupModal;
using dear::TabBar;
using dear::TabItem;
using dear::WithStyleVar;
using dear::ItemTooltip;
using dear::WithID;
using dear::OverrideID;
using dear::Disabled;
using dear::Indent;

struct WithCursorPos : ScopeWrapper<WithCursorPos>
{
    WithCursorPos(ImVec2 const& pos) noexcept : ScopeWrapper(true) { ImGui::SetCursorPos(pos); }
    WithCursorPos(float x, float y) noexcept : WithCursorPos(ImVec2(x, y)) { }
    void dtor() noexcept { ImGui::SetCursorPos(restore); ImGui::GetCurrentWindow()->DC.IsSetPos = false; }
    ImVec2 const restore = ImGui::GetCursorPos();
};
struct WithCursorScreenPos : ScopeWrapper<WithCursorScreenPos>
{
    WithCursorScreenPos(ImVec2 const& pos) noexcept : ScopeWrapper(true) { ImGui::SetCursorScreenPos(pos); }
    WithCursorScreenPos(float x, float y) noexcept : WithCursorScreenPos(ImVec2(x, y)) { }
    void dtor() noexcept { ImGui::SetCursorScreenPos(restore); ImGui::GetCurrentWindow()->DC.IsSetPos = false; }
    ImVec2 const restore = ImGui::GetCursorScreenPos();
};
struct WithCursorOffset : ScopeWrapper<WithCursorOffset>
{
    WithCursorOffset(ImVec2 const& offset) noexcept : ScopeWrapper(true) { ImGui::SetCursorScreenPos(restore + offset); }
    WithCursorOffset(float x, float y) noexcept : WithCursorOffset(ImVec2(x, y)) { }
    void dtor() noexcept { ImGui::SetCursorScreenPos(restore); ImGui::GetCurrentWindow()->DC.IsSetPos = false; }
    ImVec2 const restore = ImGui::GetCursorScreenPos();
};
struct WithStyleVarX : ScopeWrapper<WithStyleVarX>
{
    WithStyleVarX(ImGuiStyleVar idx, float val_x) noexcept : ScopeWrapper(true) { ImGui::PushStyleVarX(idx, val_x); }
    static void dtor() noexcept { ImGui::PopStyleVar(); }
};
struct WithStyleVarY : ScopeWrapper<WithStyleVarY>
{
    WithStyleVarY(ImGuiStyleVar idx, float val_y) noexcept : ScopeWrapper(true) { ImGui::PushStyleVarY(idx, val_y); }
    static void dtor() noexcept { ImGui::PopStyleVar(); }
};
struct WithColorVar : ScopeWrapper<WithColorVar>
{
    WithColorVar(ImGuiCol idx, ImVec4 const& col) noexcept : ScopeWrapper(true) { ImGui::PushStyleColor(idx, col); }
    WithColorVar(ImGuiStyleVar idx, ImU32 col) noexcept : ScopeWrapper(true) { ImGui::PushStyleColor(idx, col); }
    static void dtor() noexcept { ImGui::PopStyleColor(); }
};
struct Font : ScopeWrapper<Font>
{
    Font(ImFont* font, float size = 0.0f) noexcept : ScopeWrapper(true) { ImGui::PushFont(font, size); }
    Font(float size) noexcept : ScopeWrapper(true) { ImGui::PushFont(nullptr, size); }
    static void dtor() noexcept { ImGui::PopFont(); }
};
struct PopupContextItem : ScopeWrapper<PopupContextItem>
{
    PopupContextItem(const char* str_id = NULL, ImGuiPopupFlags popup_flags = 1) noexcept : ScopeWrapper(ImGui::BeginPopupContextItem(str_id, popup_flags)) { }
    static void dtor() noexcept { ImGui::EndPopup(); }
};
struct TableBackgroundChannel : ScopeWrapper<TableBackgroundChannel>
{
    TableBackgroundChannel() noexcept : ScopeWrapper(true) { ImGui::TablePushBackgroundChannel(); }
    static void dtor() noexcept { ImGui::TablePopBackgroundChannel(); }
};
struct TableList : ScopeWrapper<TableList, true>
{
    TableList(char const* str_id, int columns, ImGuiTableFlags flags = 0, ImVec2 outer_size = { }, float inner_width = 0.0f) : ScopeWrapper((
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImGui::GetStyle().FramePadding),
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImGui::GetStyle().ItemSpacing / 2),
        ImGui::BeginTable(str_id, columns, flags | ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_Hideable | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Sortable, outer_size, inner_width))) { }
    void dtor() const noexcept
    {
        if (ok_)
            ImGui::EndTable();
        ImGui::PopStyleVar(2);
    }
};
struct TableDockLeft : TableDockBase<TableDockLeft>
{
    TableDockLeft(char const* str_id, ImVec2 size = { -FLT_MIN, 0 }) noexcept : TableDockBase(str_id, 2, size) { }
    static void ctor() noexcept
    {
        ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Fill", ImGuiTableColumnFlags_WidthStretch);
    }
};
struct TableDockRight : TableDockBase<TableDockRight>
{
    TableDockRight(char const* str_id, ImVec2 size = { -FLT_MIN, 0 }) noexcept : TableDockBase(str_id, 2, size) { }
    static void ctor() noexcept
    {
        ImGui::TableSetupColumn("Fill", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthFixed);
    }
};
struct TableDockLeftRight : TableDockBase<TableDockLeftRight>
{
    TableDockLeftRight(char const* str_id, ImVec2 size = { -FLT_MIN, 0 }) noexcept : TableDockBase(str_id, 3, size) { }
    static void ctor() noexcept
    {
        ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Fill", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthFixed);
    }
};
struct RightAligned : ScopeWrapper<RightAligned>
{
    RightAligned(void const* id) noexcept : ScopeWrapper(ImGui::BeginTable(std::format("##RightAligned-{}", (uintptr_t)id).c_str(), 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoClip, { -FLT_MIN, 0 }))
    {
        if (ok_)
        {
            ImGui::TableSetupColumn("Fill", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
        }
    }
    static void dtor() noexcept { ImGui::EndTable(); }
};
struct DisableMarkup : ScopeWrapper<DisableMarkup>
{
    DisableMarkup() noexcept;
    static void dtor() noexcept;
};

}

}
