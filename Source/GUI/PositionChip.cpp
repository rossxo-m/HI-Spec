#include "PositionChip.h"
#include "Assets/chrome_gradient.h"

namespace hispec
{
    namespace
    {
        // [row][col] — row in {Pre,Post,FB}, col in {Global, PerBand}.
        // The (FB, Global) slot is marked Off and flagged disabled.
        constexpr ids::Position kCellPosition[3][2] = {
            { ids::Position::GlobalPre,  ids::Position::PerBandPre  },
            { ids::Position::GlobalPost, ids::Position::PerBandPost },
            { ids::Position::Off,        ids::Position::PerBandFB   }
        };

        constexpr bool kCellEnabled[3][2] = {
            { true,  true  },
            { true,  true  },
            { false, true  }
        };

        constexpr int kRows = 3;
        constexpr int kCols = 2;
    }

    PositionChip::PositionChip (APVTS& a, juce::String pid)
        : apvts (a), paramID (std::move (pid))
    {
        param = apvts.getParameter (paramID);
        jassert (param != nullptr);
        setInterceptsMouseClicks (true, false);
        refresh();
    }

    ids::Position PositionChip::getCurrent() const
    {
        if (param == nullptr) return ids::Position::Off;
        auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param);
        if (choice == nullptr) return ids::Position::Off;
        const int idx = choice->getIndex();
        return static_cast<ids::Position> (idx);
    }

    bool PositionChip::isCellEnabled (Row r, Col c) noexcept
    {
        return kCellEnabled[(int) r][(int) c];
    }

    ids::Position PositionChip::positionForCell (Row r, Col c) noexcept
    {
        return kCellPosition[(int) r][(int) c];
    }

    juce::String PositionChip::shortLabel (ids::Position p)
    {
        switch (p)
        {
            case ids::Position::GlobalPre:   return "G Pre";
            case ids::Position::GlobalPost:  return "G Post";
            case ids::Position::PerBandPre:  return "B Pre";
            case ids::Position::PerBandPost: return "B Post";
            case ids::Position::PerBandFB:   return "B FB";
            case ids::Position::Off:         return "Off";
        }
        return "?";
    }

    bool PositionChip::setRowCol (Row r, Col c)
    {
        if (! isCellEnabled (r, c)) return false;
        writePosition (positionForCell (r, c));
        return true;
    }

    void PositionChip::cycleNext()
    {
        // Build an ordered list of enabled positions (reading row-major).
        std::array<ids::Position, 5> order {};
        int n = 0;
        for (int r = 0; r < kRows; ++r)
            for (int c = 0; c < kCols; ++c)
                if (kCellEnabled[r][c])
                    order[(size_t) n++] = kCellPosition[r][c];

        const auto current = getCurrent();
        int idx = 0;
        for (int i = 0; i < n; ++i)
            if (order[(size_t) i] == current) { idx = i; break; }
        idx = (idx + 1) % n;
        writePosition (order[(size_t) idx]);
    }

    void PositionChip::writePosition (ids::Position p)
    {
        auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param);
        if (choice == nullptr) return;
        const int idx = static_cast<int> (p);
        choice->beginChangeGesture();
        choice->setValueNotifyingHost (choice->convertTo0to1 ((float) idx));
        choice->endChangeGesture();
        refresh();
    }

    void PositionChip::refresh()
    {
        currentLabel = shortLabel (getCurrent());
        repaint();
    }

    void PositionChip::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat().reduced (1.0f);
        palette::paintAquaGel (g, area, palette::colours::aqua, juce::jmin (area.getHeight() * 0.5f, 10.0f));

        g.setColour (palette::colours::chromeLo);
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (10.0f, area.getHeight() * 0.45f)))
                       .withStyle (juce::Font::bold));
        g.drawText (currentLabel, area.toNearestInt(), juce::Justification::centred);
    }

    void PositionChip::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            // Build 3×2 grid via section labels — a real grid UI comes later.
            int id = 1;
            auto add = [&] (Row r, Col c)
            {
                const auto pos = positionForCell (r, c);
                const auto enabled = isCellEnabled (r, c);
                menu.addItem (id++, shortLabel (pos),
                              enabled,
                              pos == getCurrent());
            };
            menu.addSectionHeader ("Global | Per-Band");
            add (Row::Pre,  Col::Global); add (Row::Pre,  Col::PerBand);
            add (Row::Post, Col::Global); add (Row::Post, Col::PerBand);
            add (Row::FB,   Col::Global); add (Row::FB,   Col::PerBand);

            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                [this] (int chosen)
                {
                    if (chosen <= 0) return;
                    // 1..6 map to row-major cells.
                    const int i = chosen - 1;
                    const auto r = static_cast<Row> (i / 2);
                    const auto c = static_cast<Col> (i % 2);
                    setRowCol (r, c);
                });
            return;
        }
        cycleNext();
    }
}
