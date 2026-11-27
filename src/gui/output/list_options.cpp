#include "format_options.h"
#include "../../core/app_state.h"
#include "../../logic/list_input.h"
#include "../../logic/logic.h"
#include "../gui.h"
#include "../widgets.h"
#include "generation_task.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using CSPRNG::ListField;

static constexpr float LIST_BOX_LINES = 8.0f;
static constexpr int WEIGHT_TABLE_ROWS = 10;
static constexpr float SEPARATOR_INPUT_WIDTH = 80.0f;
static constexpr float OPTION_GROUP_SPACING = 24.0f;
static constexpr uint64_t BASIS_POINTS_IN_WHOLE = 10000;
static constexpr uint32_t BASIS_POINTS_PER_PERCENT = 100;
static constexpr size_t ITEM_DISPLAY_MAX_BYTES = 200;
static constexpr unsigned char UTF8_CONTINUATION_MASK = 0xC0;
static constexpr unsigned char UTF8_CONTINUATION_BITS = 0x80;
static constexpr char LIST_DOES_NOT_FIT_HEADING[] = "List not changed";

struct ListView {
  uint64_t revision = UINT64_MAX;
  size_t textLength = 0;
  std::vector<ListField> fields;
  std::vector<size_t> itemFields;
  std::vector<uint64_t> weights;
  std::vector<uint32_t> chanceBasisPoints;
  uint64_t totalWeight = 0;
};

static ListView s_listView;

static void ComputeChances(ListView &view) {
  view.totalWeight = 0;
  for (uint64_t weight : view.weights)
    view.totalWeight += weight;
  view.chanceBasisPoints.assign(view.weights.size(), 0);
  if (view.totalWeight == 0)
    return;

  std::vector<std::pair<uint64_t, size_t>> remainders;
  remainders.reserve(view.weights.size());
  uint64_t assignedBasisPoints = 0;
  for (size_t i = 0; i < view.weights.size(); i++) {
    const uint64_t scaled = view.weights[i] * BASIS_POINTS_IN_WHOLE;
    view.chanceBasisPoints[i] = static_cast<uint32_t>(scaled / view.totalWeight);
    assignedBasisPoints += view.chanceBasisPoints[i];
    remainders.push_back({scaled % view.totalWeight, i});
  }
  const size_t leftover = static_cast<size_t>(BASIS_POINTS_IN_WHOLE - assignedBasisPoints);
  const auto largerRemainderFirst = [](const std::pair<uint64_t, size_t> &a,
                                       const std::pair<uint64_t, size_t> &b) {
    return a.first != b.first ? a.first > b.first : a.second < b.second;
  };
  std::partial_sort(remainders.begin(),
                    remainders.begin() + std::min(leftover, remainders.size()),
                    remainders.end(), largerRemainderFirst);
  for (size_t i = 0; i < leftover && i < remainders.size(); i++)
    view.chanceBasisPoints[remainders[i].second]++;
}

static const ListView &CurrentListView() {
  const std::string_view text = CSPRNG::ListTextOf(g_state);
  if (s_listView.revision == g_state.listRevision && s_listView.textLength == text.size())
    return s_listView;
  s_listView.textLength = text.size();
  s_listView.fields = CSPRNG::SplitList(text, CSPRNG::ListFormatOf(g_state));
  s_listView.itemFields = CSPRNG::ListItemFieldIndices(s_listView.fields);
  s_listView.weights =
      CSPRNG::ListPickWeights(g_state, text, s_listView.fields, s_listView.itemFields);
  ComputeChances(s_listView);
  s_listView.revision = g_state.listRevision;
  return s_listView;
}

void ListInputChanged() {
  g_state.listRevision++;
  g_state.listStatusText.clear();
  UpdateTargetEntropy();
}

bool ReplaceListText(const std::vector<char> &newText) {
  if (newText.size() >= g_state.listText.size())
    return false;
  WipeTextInputBuffers();
  WipeInputTextBufferKeepingItsSize(g_state.listText);
  if (!newText.empty())
    memcpy(g_state.listText.data(), newText.data(), newText.size());
  ListInputChanged();
  return true;
}

static void ReplaceListTextOrReport(std::vector<char> &newText) {
  if (!ReplaceListText(newText))
    ShowErrorModal(LIST_DOES_NOT_FIT_HEADING,
                   "The changed list is longer than the list box holds, so the list "
                   "was not changed.\nUse a shorter separator.");
  Crypto::SecureClearVector(newText);
}

static void RewriteListInOrder(const std::vector<size_t> &fieldOrder) {
  const ListView &view = CurrentListView();
  std::vector<char> rewritten = CSPRNG::JoinListItems(
      CSPRNG::ListTextOf(g_state), view.fields, fieldOrder,
      CSPRNG::ListJoiner(CSPRNG::ListFormatOf(g_state)));
  ReplaceListTextOrReport(rewritten);
}

static void SortList(bool descending) {
  const ListView &view = CurrentListView();
  std::vector<size_t> fieldOrder = view.itemFields;
  CSPRNG::SortListFields(CSPRNG::ListTextOf(g_state), view.fields, fieldOrder, descending);
  RewriteListInOrder(fieldOrder);
}

static std::string CountText(size_t count, const char *singular, const char *plural) {
  return WithThousandsSeparators(count) + " " + (count == 1 ? singular : plural);
}

static void RemoveDuplicateItems() {
  const ListView &view = CurrentListView();
  const std::string_view text = CSPRNG::ListTextOf(g_state);
  const std::vector<uint8_t> duplicates = CSPRNG::DuplicateListFields(text, view.fields);
  const size_t duplicateCount =
      static_cast<size_t>(std::count(duplicates.begin(), duplicates.end(), 1));
  if (duplicateCount == 0) {
    g_state.listStatusText = "No duplicates found";
    return;
  }
  std::vector<char> withoutDuplicates = CSPRNG::RemoveListFields(text, view.fields, duplicates);
  ReplaceListTextOrReport(withoutDuplicates);
  g_state.listStatusText =
      "Removed " + CountText(duplicateCount, "duplicate", "duplicates");
}

static void ClearList() {
  WipeTextInputBuffers();
  WipeInputTextBufferKeepingItsSize(g_state.listText);
  g_state.listItemWeights.clear();
  ListInputChanged();
}

static bool ShuffledListFitsTheBox(const ListView &view) {
  const std::string_view joiner = CSPRNG::ListJoiner(CSPRNG::ListFormatOf(g_state));
  return CSPRNG::JoinedListLength(view.fields, view.itemFields, joiner) < g_state.listText.size();
}

static void ShuffleListInPlace(const ListView &view) {
  if (!ShuffledListFitsTheBox(view)) {
    ShowErrorModal(LIST_DOES_NOT_FIT_HEADING,
                   "The shuffled list would be longer than the list box holds.\n"
                   "Use a shorter separator.");
    return;
  }
  StartListShuffle();
}

static void RenderShuffleInPlaceButton(const ListView &view) {
  const bool hasMinimumEntropy = g_state.hasMinimumEntropy();
  const bool canShuffle =
      view.itemFields.size() >= 2 && hasMinimumEntropy && !g_state.selfTestFailed;
  ImGui::SameLine();
  ImGui::BeginDisabled(!canShuffle);
  if (ImGui::Button("Shuffle"))
    ShuffleListInPlace(view);
  ImGui::EndDisabled();
  if (!hasMinimumEntropy)
    ImGui::SetItemTooltip("Collect at least %.0f bits of entropy before shuffling.",
                          AppConfig::MINIMUM_ENTROPY_BITS);
  else
    ImGui::SetItemTooltip(
        "Puts the items in the list box in a random order, joined with the\n"
        "separator. It draws from the generator like an output: TRUE RANDOM\n"
        "when the true random reserve covers it, PSEUDO-RANDOM otherwise.");
}

static void RenderListButtons(const ListView &view, bool withShuffleButton) {
  const bool hasItems = !view.itemFields.empty();
  ImGui::BeginDisabled(!hasItems);
  if (ImGui::Button("Sort A-Z"))
    SortList(false);
  ImGui::SetItemTooltip("Sorts the items in alphanumeric order: letters ignore case and\n"
                        "numbers inside items compare by value, so item2 comes before item10.");
  ImGui::SameLine();
  if (ImGui::Button("Sort Z-A"))
    SortList(true);
  ImGui::EndDisabled();

  if (withShuffleButton)
    RenderShuffleInPlaceButton(view);

  ImGui::SameLine();
  ImGui::BeginDisabled(!hasItems);
  if (ImGui::Button("Remove Duplicates"))
    RemoveDuplicateItems();
  ImGui::SetItemTooltip("Removes every repeat of an item and keeps its first copy.");
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::BeginDisabled(g_state.listText[0] == '\0');
  if (ImGui::Button("Clear List"))
    ClearList();
  ImGui::EndDisabled();

  if (!g_state.listStatusText.empty()) {
    ImGui::SameLine();
    ImGui::TextColored(UiColor::DimText, "%s", g_state.listStatusText.c_str());
  }
}

static void RenderListTextBox(const ListView &view) {
  ImGui::Spacing();
  if (CSPRNG::ListJoinsWithLineBreaks(CSPRNG::ListFormatOf(g_state)))
    ImGui::TextUnformatted("Enter one item per line:");
  else
    ImGui::Text("Enter items separated by \"%s\" or line breaks:", g_state.listSeparator.data());
  ImGui::SameLine();
  ImGui::TextColored(UiColor::Highlight, "%s",
                     CountText(view.itemFields.size(), "item", "items").c_str());
  ImGui::SameLine();
  ImGui::TextColored(UiColor::DimText, "(List is NOT logged)");

  const float boxHeight = ImGui::GetTextLineHeight() * LIST_BOX_LINES +
                          ImGui::GetStyle().FramePadding.y * 2.0f;
  if (ImGui::InputTextMultiline("##ListItems", g_state.listText.data(), g_state.listText.size(),
                                ImVec2(-1, boxHeight)))
    ListInputChanged();
}

static void RenderListItemsTab(bool withShuffleButton) {
  RenderListTextBox(CurrentListView());
  RenderListButtons(CurrentListView(), withShuffleButton);
}

static void SetListWeight(std::string_view item, int weight) {
  ListItemWeights &weights = g_state.listItemWeights;
  const auto found = weights.find(item);
  if (weight == AppConfig::LIST_DEFAULT_WEIGHT) {
    if (found != weights.end())
      weights.erase(found);
    return;
  }
  if (found != weights.end())
    found->second = weight;
  else
    weights.emplace(std::piecewise_construct, std::forward_as_tuple(item),
                    std::forward_as_tuple(weight));
}

static void RenderItemForDisplay(std::string_view item) {
  if (item.size() <= ITEM_DISPLAY_MAX_BYTES) {
    ImGui::TextUnformatted(item.data(), item.data() + item.size());
    return;
  }
  size_t end = ITEM_DISPLAY_MAX_BYTES;
  while (end > 0 && (static_cast<unsigned char>(item[end]) & UTF8_CONTINUATION_MASK) ==
                        UTF8_CONTINUATION_BITS)
    end--;
  ImGui::TextUnformatted(item.data(), item.data() + end);
  ImGui::SameLine(0.0f, 0.0f);
  ImGui::TextDisabled("...");
}

static void RenderChance(const ListView &view, size_t itemIndex) {
  const uint32_t basisPoints = view.chanceBasisPoints[itemIndex];
  if (view.weights[itemIndex] == 0)
    ImGui::TextDisabled("0%%");
  else if (basisPoints == 0)
    ImGui::TextUnformatted("<0.01%");
  else
    ImGui::Text("%u.%02u%%", basisPoints / BASIS_POINTS_PER_PERCENT,
                basisPoints % BASIS_POINTS_PER_PERCENT);
}

static bool RenderWeightRow(const ListView &view, std::string_view text, int row) {
  const size_t itemIndex = static_cast<size_t>(row);
  const std::string_view item = CSPRNG::ListItemText(text, view.fields[view.itemFields[itemIndex]]);
  bool changed = false;

  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::TextDisabled("%d", row + 1);
  ImGui::TableSetColumnIndex(1);
  RenderItemForDisplay(item);
  ImGui::TableSetColumnIndex(2);
  ImGui::PushID(row);
  int weight = static_cast<int>(view.weights[itemIndex]);
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::BeginDisabled(!g_state.listUseWeights);
  if (ImGui::InputInt("##Weight", &weight, 0, 0)) {
    SetListWeight(item, std::clamp(weight, AppConfig::LIST_MIN_WEIGHT, AppConfig::LIST_MAX_WEIGHT));
    changed = true;
  }
  ImGui::EndDisabled();
  ImGui::PopID();
  ImGui::TableSetColumnIndex(3);
  RenderChance(view, itemIndex);
  return changed;
}

static bool RenderWeightTable(const ListView &view) {
  const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV;
  const float tableHeight = ImGui::GetFrameHeightWithSpacing() * (WEIGHT_TABLE_ROWS + 1);
  const float numberWidth = ImGui::CalcTextSize("000000").x;
  const float weightWidth = ImGui::CalcTextSize("0000000").x + ImGui::GetStyle().FramePadding.x * 2;
  const float chanceWidth = ImGui::CalcTextSize("100.00%").x;
  bool changed = false;
  if (!ImGui::BeginTable("##ListWeights", 4, flags, ImVec2(-1, tableHeight)))
    return false;
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, numberWidth);
  ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("Weight", ImGuiTableColumnFlags_WidthFixed, weightWidth);
  ImGui::TableSetupColumn("Chance", ImGuiTableColumnFlags_WidthFixed, chanceWidth);
  ImGui::TableHeadersRow();

  const std::string_view text = CSPRNG::ListTextOf(g_state);
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(view.itemFields.size()));
  while (clipper.Step()) {
    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
      if (RenderWeightRow(view, text, row))
        changed = true;
    }
  }
  ImGui::EndTable();
  return changed;
}

static void RenderWeightTotals(const ListView &view) {
  if (view.totalWeight == 0) {
    ImGui::PushStyleColor(ImGuiCol_Text, UiColor::Warning);
    ImGui::TextWrapped("Every weight is 0, so no item can be picked. Give at least one item a "
                       "weight above 0.");
    ImGui::PopStyleColor();
    return;
  }
  ImGui::TextColored(UiColor::LabelText, "Total chance:");
  ImGui::SameLine();
  ImGui::TextColored(UiColor::Highlight, "100.00%%");
  InlineDivider();
  ImGui::TextColored(UiColor::LabelText, "Total weight: %s",
                     WithThousandsSeparators(view.totalWeight).c_str());
  if (!CSPRNG::ListPicksCanRepeat(g_state) && g_state.outputQuantity > 1) {
    ImGui::TextColored(UiColor::DimText,
                       "Chances are for the first pick: later picks in the same output leave "
                       "out the items already picked.");
  }
}

static void RenderWeightsTab() {
  ImGui::Spacing();
  if (ImGui::Checkbox("Use weights", &g_state.listUseWeights))
    ListInputChanged();
  HelpMarker("A weight sets how likely an item is to be picked compared with the\n"
             "others: weight 2 is twice as likely as weight 1, and weight 0 is never\n"
             "picked. Chance = weight / total weight, so the chances add up to 100%.\n"
             "Repeated items share one weight, and each copy counts. A weight\n"
             "belongs to the item's text, so it comes back when that item does.\n"
             "Untick to pick every item with the same chance again; the weights are kept.");
  ImGui::SameLine(0.0f, OPTION_GROUP_SPACING);
  ImGui::BeginDisabled(!g_state.listUseWeights || g_state.listItemWeights.empty());
  if (ImGui::Button("Reset Weights")) {
    g_state.listItemWeights.clear();
    ListInputChanged();
  }
  ImGui::EndDisabled();

  const ListView &view = CurrentListView();
  if (view.itemFields.empty()) {
    ImGui::TextColored(UiColor::DimText, "Add items on the Items tab to see their weights.");
    return;
  }
  if (RenderWeightTable(view))
    ListInputChanged();
  RenderWeightTotals(CurrentListView());
}

static void RenderSplitOptions() {
  bool changed = false;
  BeginOptionRow("Split items:");
  if (ImGui::Checkbox("One item per line", &g_state.listOneItemPerLine))
    changed = true;

  ImGui::SameLine(0.0f, OPTION_GROUP_SPACING);
  ImGui::BeginDisabled(g_state.listOneItemPerLine);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Separator:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(SEPARATOR_INPUT_WIDTH);
  if (ImGui::InputText("##ListSeparator", g_state.listSeparator.data(),
                       g_state.listSeparator.size()))
    changed = true;
  ImGui::EndDisabled();
  HelpMarker("Untick One item per line to split items at this separator as well as at\n"
             "line breaks. Sort, Shuffle and Shuffle List join the items with the\n"
             "separator exactly as typed.");

  ImGui::SameLine(0.0f, OPTION_GROUP_SPACING);
  if (ImGui::Checkbox("Trim spaces", &g_state.listTrimSpaces))
    changed = true;
  HelpMarker("Ignores spaces and tabs at the start and end of each item, and around\n"
             "the separator. Empty items are always skipped.");

  if (changed)
    ListInputChanged();
}

static void RenderPickingOptions() {
  BeginOptionRow("Picking:");
  ImGui::BeginDisabled(g_state.listRemovePicked);
  bool allowRepeats = CSPRNG::ListPicksCanRepeat(g_state);
  if (ImGui::Checkbox("Allow repeats", &allowRepeats)) {
    g_state.listAllowRepeats = allowRepeats;
    UpdateTargetEntropy();
  }
  ImGui::EndDisabled();
  ImGui::SetItemTooltip("Lets the same item come up more than once in one output.\n"
                        "Off: each item is picked at most once per output.");

  ImGui::SameLine(0.0f, OPTION_GROUP_SPACING);
  if (ImGui::Checkbox("Remove picked items from the list", &g_state.listRemovePicked))
    UpdateTargetEntropy();
  HelpMarker("After each Generate, the picked items are taken out of the list box,\n"
             "so the next Generate picks from the items left. No item repeats.");
}

void RenderListPickOptions() {
  BeginOptionRow("List:");
  if (ImGui::BeginTabBar("##ListTabs")) {
    if (ImGui::BeginTabItem("Items")) {
      RenderListItemsTab(true);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Advanced")) {
      RenderWeightsTab();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  RenderSplitOptions();
  RenderPickingOptions();
}

void RenderListShuffleOptions() {
  BeginOptionRow("List:");
  RenderListItemsTab(false);
  RenderSplitOptions();
}
