#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters/ParameterIDs.h"

using Catch::Approx;

static bool closeTo (int a, int b, int tol = 4) { return std::abs (a - b) <= tol; }

TEST_CASE ("PluginEditor: default layout at 1100x700", "[gui][editor]")
{
    hispec::HISpecAudioProcessor proc;
    hispec::HISpecAudioProcessorEditor ed (proc);

    REQUIRE (ed.getWidth()  == 1100);
    REQUIRE (ed.getHeight() == 700);

    // Header docked to top.
    auto& h = ed.getHeader();
    REQUIRE (h.getY() == 0);
    REQUIRE (h.getBottom() > 40);

    // Breadcrumb under header.
    auto& bc = ed.getBreadcrumb();
    REQUIRE (closeTo (bc.getY(), h.getBottom(), 2));

    // Spectrum wider than half the editor and below breadcrumb.
    auto& sp = ed.getSpectrum();
    REQUIRE (sp.getWidth()  >= 1000);
    REQUIRE (sp.getY()      >= bc.getBottom() - 2);

    // Band strip sits below the spectrum, above the FX row.
    auto& bs = ed.getBandStrip();
    REQUIRE (bs.getY() > sp.getY());

    // FX row: two equal-width cards at the bottom.
    auto& vc = ed.getVocoderCard();
    auto& fc = ed.getFilterCard();
    REQUIRE (vc.getHeight() >= 200);
    REQUIRE (fc.getHeight() >= 200);
    REQUIRE (closeTo (vc.getHeight(), fc.getHeight(), 2));
    REQUIRE (vc.getX() < fc.getX());
    REQUIRE (vc.getBottom() <= 700);
    REQUIRE (fc.getBottom() <= 700);
}

TEST_CASE ("PluginEditor: Simple mode collapses fx-row", "[gui][editor]")
{
    hispec::HISpecAudioProcessor proc;
    hispec::HISpecAudioProcessorEditor ed (proc);

    const int spectrumHBefore = ed.getSpectrum().getHeight();
    REQUIRE (ed.getVocoderCard().isVisible());
    REQUIRE (ed.getFilterCard().isVisible());
    REQUIRE (ed.getBandStrip().isVisible());

    ed.setSimpleMode (true);

    REQUIRE (! ed.getVocoderCard().isVisible());
    REQUIRE (! ed.getFilterCard().isVisible());
    REQUIRE (! ed.getBandStrip().isVisible());
    REQUIRE (ed.getSpectrum().getHeight() > spectrumHBefore);
}

TEST_CASE ("PluginEditor: A/B swap restores other slot", "[gui][editor]")
{
    hispec::HISpecAudioProcessor proc;
    hispec::HISpecAudioProcessorEditor ed (proc);

    auto* voc = proc.getAPVTS().getParameter (hispec::ids::voc_power);
    REQUIRE (voc != nullptr);

    // Set voc_power = 1 in slot A (current), then swap to empty B.
    voc->beginChangeGesture();
    voc->setValueNotifyingHost (1.0f);
    voc->endChangeGesture();
    REQUIRE (voc->getValue() > 0.5f);

    // Move to slot B — proc state will be what B holds (empty → no change? B is empty).
    // Since B is empty we actually keep current state. So force-populate B first.
    ed.copyAtoB();        // B := current (voc=1)
    voc->beginChangeGesture();
    voc->setValueNotifyingHost (0.0f);
    voc->endChangeGesture();   // A (current) now has voc=0
    REQUIRE (voc->getValue() < 0.5f);

    ed.selectB();        // current param set should now reflect B (voc=1)
    REQUIRE (voc->getValue() > 0.5f);

    ed.selectA();        // back to A (voc=0)
    REQUIRE (voc->getValue() < 0.5f);
}
