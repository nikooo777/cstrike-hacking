#include "ui/wheel.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "ui/wheel_geometry.h"

namespace ui {

namespace {

using geometry::Sector;

constexpr float kArcStep = geometry::kTwoPi / 180.0f;
constexpr float kDashPixels = 7.0f;
constexpr float kDashGapPixels = 5.0f;
constexpr int kSummaryLines = 3;
constexpr int kCaptionLines = 2;
constexpr float kReadoutWidth = 184.0f;
constexpr float kLedRadius = 3.0f;
constexpr float kCrosshairGap = 4.0f;
constexpr float kCrosshairLength = 7.0f;
constexpr float kCaptionPaddingX = 12.0f;
constexpr float kCaptionPaddingY = 7.0f;
constexpr int kVignetteSteps = 120;

ImVec2 Polar(ImVec2 center, float radius, float angle) {
    return ImVec2(center.x + radius * std::cos(angle),
                  center.y + radius * std::sin(angle));
}

int ArcSteps(float from, float to) {
    return std::max(1, static_cast<int>(std::ceil((to - from) / kArcStep)));
}

// One ring segment: matching inner and outer edge points, with the outer
// corners cut at 45 degrees.
struct Band {
    std::vector<ImVec2> inner;
    std::vector<ImVec2> outer;
};

Band MakeBand(ImVec2 center, float innerRadius, float outerRadius,
              const Sector &sector) {
    Band band;
    if (sector.end <= sector.start) {
        return band;
    }

    const float knee =
        std::min(theme::kChamfer / outerRadius, 0.5f * (sector.end - sector.start));
    const int steps = ArcSteps(sector.start, sector.end);
    std::vector<float> angles;
    for (int step = 0; step <= steps; ++step) {
        angles.push_back(sector.start + (sector.end - sector.start) *
                                            static_cast<float>(step) /
                                            static_cast<float>(steps));
    }
    angles.push_back(sector.start + knee);
    angles.push_back(sector.end - knee);
    std::sort(angles.begin(), angles.end());

    for (const float angle : angles) {
        const float fromEnd =
            std::min(angle - sector.start, sector.end - angle) * outerRadius;
        const float cut = std::max(0.0f, theme::kChamfer - fromEnd);
        band.outer.push_back(Polar(center, outerRadius - cut, angle));
        band.inner.push_back(Polar(center, innerRadius, angle));
    }
    return band;
}

void FillBand(ImDrawList *drawList, const Band &band, ImU32 color) {
    if (band.outer.size() < 2 || (color & IM_COL32_A_MASK) == 0) {
        return;
    }

    const int count = static_cast<int>(band.outer.size());
    const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    drawList->PrimReserve((count - 1) * 6, count * 2);
    const auto base = static_cast<ImDrawIdx>(drawList->_VtxCurrentIdx);
    for (int point = 0; point < count; ++point) {
        drawList->PrimWriteVtx(band.inner[point], uv, color);
        drawList->PrimWriteVtx(band.outer[point], uv, color);
    }
    for (int point = 0; point + 1 < count; ++point) {
        const auto first = static_cast<ImDrawIdx>(base + point * 2);
        drawList->PrimWriteIdx(first);
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(first + 1));
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(first + 3));
        drawList->PrimWriteIdx(first);
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(first + 3));
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(first + 2));
    }
}

void OutlineBand(ImDrawList *drawList, const Band &band, ImU32 color) {
    if (band.outer.size() < 2) {
        return;
    }
    std::vector<ImVec2> points = band.outer;
    points.insert(points.end(), band.inner.rbegin(), band.inner.rend());
    drawList->AddPolyline(points.data(), static_cast<int>(points.size()), color,
                          ImDrawFlags_Closed, theme::kHairline);
}

void EdgeBand(ImDrawList *drawList, const Band &band, ImU32 color) {
    if (band.outer.size() < 2) {
        return;
    }
    drawList->AddPolyline(band.outer.data(), static_cast<int>(band.outer.size()),
                          color, ImDrawFlags_None, theme::kEdge);
}

void DashBand(ImDrawList *drawList, const Band &band, ImU32 color) {
    bool drawing = true;
    float remaining = kDashPixels;
    std::vector<ImVec2> dash;
    for (std::size_t point = 0; point + 1 < band.outer.size(); ++point) {
        ImVec2 from = band.outer[point];
        const ImVec2 to = band.outer[point + 1];
        float length = std::hypot(to.x - from.x, to.y - from.y);
        while (length > 0.0f) {
            const float step = std::min(remaining, length);
            const float t = step / length;
            const ImVec2 next(from.x + (to.x - from.x) * t,
                              from.y + (to.y - from.y) * t);
            if (drawing) {
                if (dash.empty()) {
                    dash.push_back(from);
                }
                dash.push_back(next);
            }
            remaining -= step;
            length -= step;
            from = next;
            if (remaining <= 0.0f) {
                if (drawing && dash.size() >= 2) {
                    drawList->AddPolyline(dash.data(),
                                          static_cast<int>(dash.size()), color,
                                          ImDrawFlags_None, theme::kEdge);
                }
                dash.clear();
                drawing = !drawing;
                remaining = drawing ? kDashPixels : kDashGapPixels;
            }
        }
    }
    if (drawing && dash.size() >= 2) {
        drawList->AddPolyline(dash.data(), static_cast<int>(dash.size()), color,
                              ImDrawFlags_None, theme::kEdge);
    }
}

// The part of `sector` already swept by an arc that starts at `origin` and
// has covered `sweep` radians.
Sector Revealed(const Sector &sector, float origin, float sweep) {
    if (sweep >= geometry::kTwoPi) {
        return sector;
    }
    float offset = geometry::NormalizeAngle(sector.start - origin);
    if (offset < 0.0f) {
        offset += geometry::kTwoPi;
    }
    if (offset >= sweep) {
        return Sector{sector.start, sector.start};
    }
    return Sector{sector.start,
                  std::min(sector.end, sector.start + (sweep - offset))};
}

ImVec2 TextSize(ImFont *font, const std::string &text) {
    return font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text.c_str());
}

void PutText(ImDrawList *drawList, ImFont *font, ImVec2 position, ImU32 color,
             const std::string &text) {
    drawList->AddText(font, font->FontSize,
                      ImVec2(std::floor(position.x), std::floor(position.y)),
                      color, text.c_str());
}

void CenteredText(ImDrawList *drawList, ImFont *font, ImVec2 anchor,
                  ImU32 color, const std::string &text) {
    const ImVec2 size = TextSize(font, text);
    PutText(drawList, font,
            ImVec2(anchor.x - 0.5f * size.x, anchor.y - 0.5f * size.y), color,
            text);
}

// Text centered on the ring at `angle`, turned to run along the ring and
// flipped in the lower half so it never reads upside down.
void RingText(ImDrawList *drawList, ImFont *font, ImVec2 center, float radius,
              float angle, ImU32 color, const std::string &text,
              ImU32 ledColor = 0) {
    const ImVec2 anchor = Polar(center, radius, angle);
    const ImVec2 size = TextSize(font, text);
    const float ledWidth = ledColor != 0 ? 3.0f * kLedRadius : 0.0f;
    const float left = anchor.x - 0.5f * (size.x + ledWidth);
    const int firstVertex = drawList->VtxBuffer.Size;
    if (ledColor != 0) {
        drawList->AddCircleFilled(ImVec2(left + kLedRadius, anchor.y),
                                  kLedRadius, ledColor, 12);
    }
    drawList->AddText(font, font->FontSize,
                      ImVec2(left + ledWidth, anchor.y - 0.5f * size.y), color,
                      text.c_str());

    float rotation = angle + 0.5f * geometry::kPi;
    if (std::sin(angle) > 0.05f) {
        rotation -= geometry::kPi;
    }
    const float cosine = std::cos(rotation);
    const float sine = std::sin(rotation);
    for (int vertex = firstVertex; vertex < drawList->VtxBuffer.Size; ++vertex) {
        ImVec2 &position = drawList->VtxBuffer.Data[vertex].pos;
        const float offsetX = position.x - anchor.x;
        const float offsetY = position.y - anchor.y;
        position = ImVec2(anchor.x + offsetX * cosine - offsetY * sine,
                          anchor.y + offsetX * sine + offsetY * cosine);
    }
}

std::vector<std::string> Wrap(ImFont *font, const std::string &text,
                              float maxWidth) {
    std::vector<std::string> lines;
    std::string line;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find(' ', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        const std::string word = text.substr(start, end - start);
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && TextSize(font, candidate).x > maxWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
        start = end + 1;
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    return lines;
}

bool IsLit(const WheelItem &item) {
    return item.state == ItemState::On || item.state == ItemState::Ok;
}

ImU32 StateColor(const WheelItem &item) {
    switch (item.state) {
    case ItemState::On:
    case ItemState::Ok:
        return theme::kAmber;
    case ItemState::Fault:
        return theme::kFault;
    default:
        return theme::kSteelFaint;
    }
}

std::string StateText(const WheelItem &item) {
    std::string text;
    switch (item.state) {
    case ItemState::On:
        text = "On";
        break;
    case ItemState::Off:
        text = "Off";
        break;
    case ItemState::Ok:
        text = "Working";
        break;
    case ItemState::Idle:
        text = "Waiting for a game";
        break;
    case ItemState::Fault:
        text = "Needs attention";
        break;
    }
    if (item.kind == ItemKind::Action) {
        text = "Click to run";
    }
    if (item.experimental) {
        text += ", experimental";
    }
    return text;
}

std::string CategoryState(const WheelCategory &category) {
    int lit = 0;
    int counted = 0;
    for (const auto &item : category.items) {
        if (item.kind == ItemKind::Action) {
            continue;
        }
        ++counted;
        lit += IsLit(item) ? 1 : 0;
    }
    const bool status = !category.items.empty() &&
                        category.items.front().kind == ItemKind::Status;
    return std::to_string(lit) + " of " + std::to_string(counted) +
           (status ? " working" : " on");
}

struct Readout {
    std::string title;
    std::string state;
    ImU32 stateColor = theme::kTextDim;
    std::string summary;
    std::vector<std::string> caption;
};

Readout ItemReadout(const WheelItem &item) {
    Readout readout;
    readout.title = item.title;
    readout.state = StateText(item);
    readout.stateColor =
        item.state == ItemState::Fault
            ? theme::kFault
            : (IsLit(item) ? theme::kAmber : theme::kTextDim);
    readout.summary = item.summary;
    readout.caption = item.details;
    return readout;
}

Readout CategoryReadout(const WheelCategory &category, const std::string &hint) {
    Readout readout;
    readout.title = category.label;
    readout.state = CategoryState(category);
    readout.summary = category.summary;
    if (!hint.empty()) {
        readout.caption = {hint};
    }
    return readout;
}

void DrawReadout(ImDrawList *drawList, const theme::Fonts &fonts,
                 ImVec2 center, const Readout &readout, float alpha) {
    CenteredText(drawList, fonts.title, ImVec2(center.x, center.y - 58.0f),
                 theme::WithAlpha(theme::kText, alpha), readout.title);
    CenteredText(drawList, fonts.small, ImVec2(center.x, center.y - 33.0f),
                 theme::WithAlpha(readout.stateColor, alpha), readout.state);

    float y = center.y + 18.0f;
    const auto summary = Wrap(fonts.label, readout.summary, kReadoutWidth);
    const int lines = std::min(kSummaryLines, static_cast<int>(summary.size()));
    for (int line = 0; line < lines; ++line) {
        const ImVec2 size = TextSize(fonts.label, summary[line]);
        PutText(drawList, fonts.label, ImVec2(center.x - 0.5f * size.x, y),
                theme::WithAlpha(theme::kTextDim, alpha), summary[line]);
        y += fonts.label->FontSize + 2.0f;
    }
}

void DrawCaption(ImDrawList *drawList, ImFont *font, ImVec2 top,
                 const std::vector<std::string> &lines, float alpha) {
    const int count = std::min(kCaptionLines, static_cast<int>(lines.size()));
    if (count == 0) {
        return;
    }
    float width = 0.0f;
    for (int line = 0; line < count; ++line) {
        width = std::max(width, TextSize(font, lines[line]).x);
    }
    const float lineHeight = font->FontSize + 2.0f;
    const float halfWidth = 0.5f * width + kCaptionPaddingX;
    const float height = count * lineHeight - 2.0f + 2.0f * kCaptionPaddingY;
    const float cut = theme::kChamfer;
    const ImVec2 shape[] = {
        ImVec2(top.x - halfWidth + cut, top.y),
        ImVec2(top.x + halfWidth - cut, top.y),
        ImVec2(top.x + halfWidth, top.y + cut),
        ImVec2(top.x + halfWidth, top.y + height - cut),
        ImVec2(top.x + halfWidth - cut, top.y + height),
        ImVec2(top.x - halfWidth + cut, top.y + height),
        ImVec2(top.x - halfWidth, top.y + height - cut),
        ImVec2(top.x - halfWidth, top.y + cut),
    };
    drawList->AddConvexPolyFilled(shape, 8,
                                  theme::WithAlpha(theme::kGlass, alpha));
    drawList->AddPolyline(shape, 8, theme::WithAlpha(theme::kSteelFaint, alpha),
                          ImDrawFlags_Closed, theme::kHairline);
    float y = top.y + kCaptionPaddingY;
    for (int line = 0; line < count; ++line) {
        const ImVec2 size = TextSize(font, lines[line]);
        PutText(drawList, font, ImVec2(top.x - 0.5f * size.x, y),
                theme::WithAlpha(theme::kSteel, alpha), lines[line]);
        y += lineHeight;
    }
}

// Dims the game behind the wheel: solid inside the category ring, fading to
// clear at kVignetteRadius. One mesh with shared rim vertices, interpolated
// per vertex, so it neither bands nor leaves a seam at the rim.
void DrawVignette(ImDrawList *drawList, ImVec2 center, float alpha) {
    const ImU32 solid = theme::WithAlpha(theme::kVignette, alpha);
    const ImU32 clear = theme::WithAlpha(theme::kVignette, 0.0f);
    const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    drawList->PrimReserve(kVignetteSteps * 9, 1 + (kVignetteSteps + 1) * 2);
    const auto base = static_cast<ImDrawIdx>(drawList->_VtxCurrentIdx);
    drawList->PrimWriteVtx(center, uv, solid);
    for (int step = 0; step <= kVignetteSteps; ++step) {
        const float angle = geometry::kTwoPi * static_cast<float>(step) /
                            static_cast<float>(kVignetteSteps);
        drawList->PrimWriteVtx(Polar(center, theme::kCategoryOuter, angle), uv,
                               solid);
        drawList->PrimWriteVtx(Polar(center, theme::kVignetteRadius, angle), uv,
                               clear);
    }
    for (int step = 0; step < kVignetteSteps; ++step) {
        const auto rim = static_cast<ImDrawIdx>(base + 1 + step * 2);
        drawList->PrimWriteIdx(base);
        drawList->PrimWriteIdx(rim);
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(rim + 2));
        drawList->PrimWriteIdx(rim);
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(rim + 1));
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(rim + 3));
        drawList->PrimWriteIdx(rim);
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(rim + 3));
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(rim + 2));
    }
}

void DrawCrosshair(ImDrawList *drawList, ImVec2 center, ImU32 color) {
    const float start = kCrosshairGap;
    const float end = kCrosshairGap + kCrosshairLength;
    drawList->AddLine(ImVec2(center.x - end, center.y),
                      ImVec2(center.x - start, center.y), color);
    drawList->AddLine(ImVec2(center.x + start, center.y),
                      ImVec2(center.x + end, center.y), color);
    drawList->AddLine(ImVec2(center.x, center.y - end),
                      ImVec2(center.x, center.y - start), color);
    drawList->AddLine(ImVec2(center.x, center.y + start),
                      ImVec2(center.x, center.y + end), color);
}

void DrawCaret(ImDrawList *drawList, ImVec2 center, float angle, ImU32 color) {
    const float tip = theme::kCategoryOuter + theme::kCaretOffset;
    const float base = tip + theme::kCaretLength;
    const float half = 0.5f * theme::kCaretWidth / base;
    drawList->AddTriangleFilled(Polar(center, tip, angle),
                                Polar(center, base, angle - half),
                                Polar(center, base, angle + half), color);
}

void DrawTicks(ImDrawList *drawList, ImVec2 center, const Sector &sector,
               const WheelCategory &category, float alpha) {
    std::vector<const WheelItem *> counted;
    for (const auto &item : category.items) {
        if (item.kind != ItemKind::Action) {
            counted.push_back(&item);
        }
    }
    if (counted.empty()) {
        return;
    }
    const float inner = theme::kCategoryOuter + theme::kTickOffset;
    const float outer = inner + theme::kTickLength;
    const float first = sector.Middle() -
                        0.5f * theme::kTickSpacing *
                            static_cast<float>(counted.size() - 1);
    for (std::size_t tick = 0; tick < counted.size(); ++tick) {
        const float angle =
            first + theme::kTickSpacing * static_cast<float>(tick);
        const ImU32 color = theme::WithAlpha(StateColor(*counted[tick]), alpha);
        drawList->AddLine(Polar(center, inner, angle),
                          Polar(center, outer, angle), color, theme::kEdge);
    }
}

geometry::Layout MakeLayout(const WheelModel &model, int selectedCategory) {
    const int categoryCount = static_cast<int>(model.categories.size());
    geometry::Layout layout{};
    layout.centerRadius = theme::kCenterRadius;
    layout.items = {theme::kItemInner, theme::kItemOuter};
    layout.categories = {theme::kCategoryInner, theme::kCategoryOuter};
    layout.categoryCount = categoryCount;
    layout.categoryGap = theme::kCategoryGap;
    layout.itemCount =
        static_cast<int>(model.categories[selectedCategory].items.size());
    layout.itemCenter = geometry::CategorySector(selectedCategory,
                                                 categoryCount,
                                                 theme::kCategoryGap)
                            .Middle();
    layout.itemSpan = theme::kItemSpan;
    layout.itemMaxSpan = theme::kItemMaxSpan;
    layout.itemGap = theme::kItemGap;
    return layout;
}

} // namespace

WheelResult DrawWheel(ImDrawList *drawList, const theme::Fonts &fonts,
                      const WheelModel &model, WheelState &state,
                      const WheelInput &input) {
    WheelResult result;
    const int categoryCount = static_cast<int>(model.categories.size());
    if (drawList == nullptr || categoryCount == 0) {
        return result;
    }

    if (input.scroll != 0.0f) {
        const int step = input.scroll > 0.0f ? categoryCount - 1 : 1;
        state.selectedCategory = (state.selectedCategory + step) % categoryCount;
        state.categoryChangedAt = input.time;
    }
    state.selectedCategory =
        std::clamp(state.selectedCategory, 0, categoryCount - 1);

    const ImVec2 center = input.center;
    const float dx = input.mouse.x - center.x;
    const float dy = input.mouse.y - center.y;
    auto hit = geometry::HitTest(MakeLayout(model, state.selectedCategory), dx, dy);
    if (input.clicked && hit.band == geometry::Band::Categories &&
        hit.index != state.selectedCategory) {
        state.selectedCategory = hit.index;
        state.categoryChangedAt = input.time;
        hit = geometry::Hit{};
    }

    const auto &selected = model.categories[state.selectedCategory];
    const geometry::Layout layout = MakeLayout(model, state.selectedCategory);
    if (input.clicked && hit.band == geometry::Band::Items &&
        selected.items[hit.index].kind != ItemKind::Status) {
        result.category = state.selectedCategory;
        result.item = hit.index;
    }

    const float open = geometry::EaseOutCubic(static_cast<float>(
        (input.time - state.openedAt) / theme::kOpenSeconds));
    const float alpha = open;
    const float sweep = open * geometry::kTwoPi;
    const float origin =
        geometry::CategorySector(0, categoryCount, 0.0f).start;
    const float categoryLabelRadius =
        0.5f * (theme::kCategoryInner + theme::kCategoryOuter);
    const float itemLabelRadius = 0.5f * (theme::kItemInner + theme::kItemOuter);

    DrawVignette(drawList, center, alpha);

    for (int category = 0; category < categoryCount; ++category) {
        const Sector full = geometry::CategorySector(category, categoryCount,
                                                     theme::kCategoryGap);
        const Sector shown = Revealed(full, origin, sweep);
        const bool isSelected = category == state.selectedCategory;
        const bool isHovered = hit.band == geometry::Band::Categories &&
                               hit.index == category;
        const Band band = MakeBand(center, theme::kCategoryInner,
                                   theme::kCategoryOuter, shown);
        FillBand(drawList, band,
                 theme::WithAlpha(isHovered ? theme::kGlassRaised
                                            : theme::kGlass,
                                  alpha));
        if (isSelected) {
            FillBand(drawList, band, theme::WithAlpha(theme::kAmberWash, alpha));
        }
        OutlineBand(drawList, band, theme::WithAlpha(theme::kSteelFaint, alpha));
        if (isSelected) {
            EdgeBand(drawList, band, theme::WithAlpha(theme::kAmber, alpha));
        }
        if (shown.end >= full.end) {
            DrawTicks(drawList, center, full, model.categories[category],
                      alpha);
            const ImU32 labelColor =
                isSelected ? theme::kAmber
                           : (isHovered ? theme::kText : theme::kSteel);
            RingText(drawList, fonts.category, center, categoryLabelRadius,
                     full.Middle(), theme::WithAlpha(labelColor, alpha),
                     model.categories[category].label);
        }
    }

    const float fan = geometry::EaseOutCubic(static_cast<float>(
        (input.time - state.categoryChangedAt) / theme::kFanSeconds));
    const float itemAlpha = alpha * fan;
    if (layout.itemCount > 0) {
        const Sector firstItem = geometry::ItemSector(
            0, layout.itemCount, layout.itemCenter, layout.itemSpan,
            layout.itemMaxSpan, 0.0f);
        const float fanSpan = std::min(
            layout.itemSpan * static_cast<float>(layout.itemCount),
            layout.itemMaxSpan);
        const float fanSweep = fan >= 1.0f ? geometry::kTwoPi : fan * fanSpan;
        for (int item = 0; item < layout.itemCount; ++item) {
            const auto &entry = selected.items[item];
            const Sector full = geometry::ItemSector(
                item, layout.itemCount, layout.itemCenter, layout.itemSpan,
                layout.itemMaxSpan, layout.itemGap);
            const Sector shown = Revealed(Revealed(full, origin, sweep),
                                          firstItem.start, fanSweep);
            const bool isHovered =
                hit.band == geometry::Band::Items && hit.index == item;
            const bool isOn = entry.kind == ItemKind::Toggle && IsLit(entry);
            const Band band =
                MakeBand(center, theme::kItemInner, theme::kItemOuter, shown);

            FillBand(drawList, band,
                     theme::WithAlpha(isHovered ? theme::kGlassRaised
                                                : theme::kGlass,
                                      itemAlpha));
            if (isOn) {
                FillBand(drawList, band,
                         theme::WithAlpha(theme::kAmberFill, itemAlpha));
            }
            OutlineBand(drawList, band,
                        theme::WithAlpha(theme::kSteelFaint, itemAlpha));
            const ImU32 edge = theme::WithAlpha(
                isOn ? theme::kAmber : theme::kSteel, itemAlpha);
            if (entry.experimental) {
                DashBand(drawList, band, edge);
            } else if (isOn) {
                EdgeBand(drawList, band, edge);
            }

            if (shown.end < full.end) {
                continue;
            }
            const ImU32 labelColor =
                isOn ? theme::kAmber
                     : (isHovered ? theme::kText : theme::kSteel);
            const ImU32 ledColor =
                entry.kind == ItemKind::Status
                    ? theme::WithAlpha(StateColor(entry), itemAlpha)
                    : 0;
            RingText(drawList, fonts.label, center, itemLabelRadius,
                     full.Middle(), theme::WithAlpha(labelColor, itemAlpha),
                     entry.label, ledColor);
        }
    }

    drawList->AddCircleFilled(center, theme::kCenterRadius,
                              theme::WithAlpha(theme::kGlass, alpha), 96);
    drawList->AddCircle(center, theme::kCenterRadius,
                        theme::WithAlpha(theme::kSteelFaint, alpha), 96,
                        theme::kHairline);
    DrawCrosshair(drawList, center, theme::WithAlpha(theme::kSteel, alpha));

    Readout readout = CategoryReadout(selected, model.hint);
    if (hit.band == geometry::Band::Items) {
        readout = ItemReadout(selected.items[hit.index]);
    } else if (hit.band == geometry::Band::Categories) {
        readout = CategoryReadout(model.categories[hit.index], model.hint);
    }
    DrawReadout(drawList, fonts, center, readout, alpha);

    if (std::hypot(dx, dy) > theme::kCenterRadius) {
        DrawCaret(drawList, center, std::atan2(dy, dx),
                  theme::WithAlpha(theme::kAmber, alpha));
    }

    DrawCaption(drawList, fonts.small,
                ImVec2(center.x, center.y + theme::kCategoryOuter +
                                     theme::kCaptionOffset),
                readout.caption, alpha);
    return result;
}

} // namespace ui
