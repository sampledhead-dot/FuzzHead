#include "PluginEditor.h"
#include "Presets.h"

static const char* kEngineNames[] = { "Deep", "Vintage", "Solo", "Strings", "Harmony" };
static const char* kEngineDesc[] = {
    "Grave y con mucho peso: seno + sub-octava, saturado suave.",
    "Cálido y redondo, carácter analógico clásico.",
    "Lead brillante y mordiente; el filtro se abre con la dinámica.",
    "Textura de cuerdas electrónicas, ataque lento y ancho.",
    "Añade hasta dos voces a intervalos fijos (semitonos)." };
static const char* kModeNames[] = { "Soft", "Hard", "Rich", "Wavefold" };

//==============================================================================
FuzzLookAndFeel::FuzzLookAndFeel()
{
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::ComboBox::backgroundColourId, col::text);
    setColour (juce::ComboBox::textColourId, col::orangeL);
    setColour (juce::ComboBox::outlineColourId, col::text);
    setColour (juce::ComboBox::arrowColourId, col::orangeL);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::headerTextColourId, col::orangeD);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, col::text);
    setColour (juce::TextButton::buttonColourId, col::text);
    setColour (juce::TextButton::buttonOnColourId, col::accent);
    setColour (juce::TextButton::textColourOffId, col::cream);
    setColour (juce::TextButton::textColourOnId, col::text);
    setColour (juce::AlertWindow::backgroundColourId, col::panel);
    setColour (juce::AlertWindow::textColourId, col::text);
    setColour (juce::AlertWindow::outlineColourId, col::accent);
    setColour (juce::TextEditor::backgroundColourId, col::cream);
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::outlineColourId, col::text);
}

void FuzzLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    // Knob como los botones redondos del logo: disco naranja, arco negro, puntero crema
    const auto arcCol = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float r = (float) juce::jmin (w, h) * 0.5f - 4.0f, cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float ang = a0 + pos * (a1 - a0);
    juce::Path bgArc; bgArc.addCentredArc (cx, cy, r, r, 0, a0, a1, true);
    g.setColour (s.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (bgArc, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (std::abs (pos) > 0.002f)
    {
        juce::Path arc; arc.addCentredArc (cx, cy, r, r, 0, juce::jmin (a0, ang), juce::jmax (a0, ang), true);
        g.setColour (arcCol);
        g.strokePath (arc, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const float kr = r * 0.68f;
    g.setColour (col::orangeD); g.fillEllipse (cx - kr, cy - kr + 2.0f, kr * 2, kr * 2);
    g.setColour (col::accent);  g.fillEllipse (cx - kr, cy - kr, kr * 2, kr * 2);
    g.setColour (col::orangeL); g.drawEllipse (cx - kr * 0.6f, cy - kr * 0.6f, kr * 1.2f, kr * 1.2f, 1.2f);
    juce::Path ptr; ptr.addRoundedRectangle (-1.75f, -kr + 2.0f, 3.5f, kr * 0.85f, 1.75f);
    g.setColour (col::cream); g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (cx, cy));
}

void FuzzLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat().withSizeKeepingCentre (42.0f, 24.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? col::accent : col::panel2);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (col::text); g.drawRoundedRectangle (r.reduced (1.0f), 11.0f, 2.0f);
    const float d = 16.0f, kx = on ? r.getRight() - d - 4.0f : r.getX() + 4.0f;
    g.fillEllipse (kx, r.getCentreY() - d / 2, d, d);
}

void FuzzLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState();
    if (b.getProperties()["pad"])
    {
        // Pad del sampler: borde naranja grueso; encendido = relleno naranja
        g.setColour (on ? col::accent : (over ? juce::Colour (0xfffbe3cf) : col::panel));
        g.fillRoundedRectangle (r, 4.0f);
        if (on) { g.setColour (col::orangeD); g.fillRect (r.removeFromBottom (3.0f).reduced (3.0f, 0.0f)); }
        else    { g.setColour (col::accent); g.drawRoundedRectangle (r, 4.0f, 2.5f); }
        return;
    }
    auto c = on ? b.findColour (juce::TextButton::buttonOnColourId) : base;
    if (over && ! on) c = c.brighter (0.15f);
    if (down) c = c.darker (0.15f);
    if (! b.isEnabled()) c = c.withAlpha (0.35f);
    g.setColour (c); g.fillRoundedRectangle (r, 5.0f);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label, juce::Colour c, bool onDark)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, onDark ? col::cream : col::text);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, onDark ? juce::Colour (0xff3d3631) : col::line);
    slider.setColour (juce::Slider::textBoxTextColourId, onDark ? col::cream.withAlpha (0.75f) : col::muted);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    juce::ignoreUnused (c);
    name.setText (label.toUpperCase().replace (u8 ("ó"), u8 ("Ó")), juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    name.setColour (juce::Label::textColourId, onDark ? col::cream : col::text);
    name.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    addAndMakeVisible (slider); addAndMakeVisible (name);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
}
void Knob::resized()
{
    auto r = getLocalBounds();
    name.setBounds (r.removeFromBottom (14));
    slider.setBounds (r);
}

void ModulePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r.withTrimmedTop (6.0f), 8.0f);
    auto plate = r.withTrimmedBottom (5.0f);
    g.setColour (col::panel); g.fillRoundedRectangle (plate, 8.0f);
    g.setColour (col::orangeD.withAlpha (0.7f));                         // tornillos
    g.fillEllipse (8.0f, plate.getBottom() - 14.0f, 6.0f, 6.0f);
    g.fillEllipse (plate.getRight() - 14.0f, plate.getBottom() - 14.0f, 6.0f, 6.0f);
    // título como pegatina negra ligeramente girada
    const juce::Font f (juce::FontOptions (14.0f, juce::Font::bold));
    juce::GlyphArrangement ga; ga.addLineOfText (f, title, 0, 0);
    const float tw = ga.getBoundingBox (0, -1, true).getWidth();
    const float tx = (float) titleX, ty = 12.0f;
    juce::Graphics::ScopedSaveState ss (g);
    g.addTransform (juce::AffineTransform::rotation (-0.025f, tx, ty));
    g.setColour (col::text); g.fillRoundedRectangle (tx, ty, tw + 18.0f, 24.0f, 3.0f);
    g.setColour (col::cream); g.setFont (f);
    g.drawText (title, (int) tx + 9, (int) ty, (int) tw + 4, 24, juce::Justification::centredLeft);
}

//==============================================================================
std::unique_ptr<juce::ToggleButton> FuzzHeadEditor::makeSwitch (const juce::String& id, juce::Colour c)
{
    auto b = std::make_unique<juce::ToggleButton>();
    b->setColour (juce::ToggleButton::tickColourId, c);
    addAndMakeVisible (*b);
    buttonAtts.push_back (std::make_unique<BA> (proc.apvts, id, *b));
    return b;
}

FuzzHeadEditor::FuzzHeadEditor (FuzzHeadProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;
    for (auto* m : { &pSynth, &pDist, &pMod, &pCrush, &pRing, &pRev }) addAndMakeVisible (*m);

    // Barra superior
    for (auto* b : { &instGuitar, &instBass }) { addAndMakeVisible (*b); b->setClickingTogglesState (false); }
    instGuitar.onClick = [this] { setParam (ids::instrument, 0); };
    instBass.onClick   = [this] { setParam (ids::instrument, 1); };
    addAndMakeVisible (presetBox);
    presetBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2b2622));
    presetBox.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff2b2622));
    for (auto* b : { &instGuitar, &instBass, &prevB, &nextB, &saveB, &delB })
        b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b2622));
    presetBox.onChange = [this] { selectPreset (presetBox.getSelectedId()); };
    for (auto* b : { &prevB, &nextB, &saveB, &delB }) addAndMakeVisible (*b);
    prevB.onClick = [this] { int i = presetBox.getSelectedItemIndex(); presetBox.setSelectedItemIndex ((i - 1 + presetBox.getNumItems()) % presetBox.getNumItems()); };
    nextB.onClick = [this] { int i = presetBox.getSelectedItemIndex(); presetBox.setSelectedItemIndex ((i + 1) % presetBox.getNumItems()); };
    saveB.onClick = [this]
    {
        auto* w = new juce::AlertWindow ("Guardar preset", "Nombre del preset:", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", proc.currentPresetName == "Default" ? juce::String() : proc.currentPresetName);
        w->addButton ("Guardar", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
        {
            const auto name = w->getTextEditorContents ("name").trim();
            if (r != 1 || name.isEmpty()) return;
            if (name == "Default") { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "FuzzHead", u8 ("\"Default\" está reservado.")); return; }
            auto doSave = [this, name] { proc.saveUserPreset (name); refreshPresets(); };
            if (proc.getUserPresetNames().contains (name))
                juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon).withTitle ("FuzzHead")
                                              .withMessage ("Ya existe \"" + name + u8 ("\". ¿Sobrescribir?")).withButton (u8 ("Sí"), 1).withButton ("No", 0).withAssociatedComponent (this),
                                              [doSave] (int ok) { if (ok == 1) doSave(); });
            else doSave();
        }), true);
    };
    delB.onClick = [this]
    {
        const auto name = proc.currentPresetName;
        if (! proc.getUserPresetNames().contains (name)) return;
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon).withTitle ("FuzzHead")
                                              .withMessage (u8 ("¿Borrar el preset \"") + name + "\"?").withButton ("Borrar", 1).withButton ("Cancelar", 0).withAssociatedComponent (this),
                                      [this, name] (int ok) { if (ok == 1) { proc.deleteUserPreset (name); proc.loadFactory (0); refreshPresets(); } });
    };

    kIn  = std::make_unique<Knob> (s, ids::inputDb, "Input", col::accent, true);
    kMix = std::make_unique<Knob> (s, ids::mix, "Mix", col::accent, true);
    kOut = std::make_unique<Knob> (s, ids::outputDb, "Output", col::accent, true);

    // Sinte
    swSynth = makeSwitch (ids::synthOn, col::synth);
    for (int i = 0; i < 5; ++i)
    {
        auto* b = engineButtons.add (new juce::TextButton (kEngineNames[i]));
        b->getProperties().set ("pad", true);
        b->setColour (juce::TextButton::textColourOffId, col::text);
        b->onClick = [this, i] { setParam (ids::engine, (float) i); };
        addAndMakeVisible (b);
    }
    engineDesc.setColour (juce::Label::textColourId, col::muted);
    engineDesc.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (engineDesc);
    kSynthMix = std::make_unique<Knob> (s, ids::synthMix, "Mix", col::synth);
    kInt1 = std::make_unique<Knob> (s, ids::int1, "Intervalo 1", col::synth);
    kInt2 = std::make_unique<Knob> (s, ids::int2, "Intervalo 2", col::synth);
    swInt2 = makeSwitch (ids::int2On, col::synth);
    int2Label.setColour (juce::Label::textColourId, col::text); int2Label.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (int2Label);

    // Distorsión
    swDist = makeSwitch (ids::distOn, col::dist);
    kGain = std::make_unique<Knob> (s, ids::distGain, "Gain", col::dist);
    for (int i = 0; i < 4; ++i)
    {
        auto* b = modeButtons.add (new juce::TextButton (kModeNames[i]));
        b->getProperties().set ("pad", true);
        b->setColour (juce::TextButton::textColourOffId, col::text);
        b->onClick = [this, i] { setParam (ids::distMode, (float) i); };
        addAndMakeVisible (b);
    }
    for (auto* b : { &preB, &postB }) { b->getProperties().set ("pad", true); b->setColour (juce::TextButton::textColourOffId, col::text); addAndMakeVisible (*b); }
    preB.onClick  = [this] { const bool pr = getParam (ids::distPre) > 0.5f, po = getParam (ids::distPost) > 0.5f; if (pr && ! po) return; setParam (ids::distPre, pr ? 0.f : 1.f); };
    postB.onClick = [this] { const bool pr = getParam (ids::distPre) > 0.5f, po = getParam (ids::distPost) > 0.5f; if (po && ! pr) return; setParam (ids::distPost, po ? 0.f : 1.f); };

    // Modulación
    swMod = makeSwitch (ids::modOn, col::mod);
    kDepth = std::make_unique<Knob> (s, ids::modDepth, "Depth", col::mod);
    kModMix = std::make_unique<Knob> (s, ids::modMix, "Mix", col::mod);

    // Grupo reordenable
    swCrush = makeSwitch (ids::crushOn, col::crush);
    swRing = makeSwitch (ids::ringOn, col::ring);
    swRev = makeSwitch (ids::revOn, col::rev);
    kCrush = std::make_unique<Knob> (s, ids::crushRate, "Crush", col::crush);
    kBits = std::make_unique<Knob> (s, ids::crushBits, u8 ("Resolución"), col::crush);
    kRingF = std::make_unique<Knob> (s, ids::ringFreq, "Freq", col::ring);
    kRingMix = std::make_unique<Knob> (s, ids::ringMix, "Mix", col::ring);
    kTime = std::make_unique<Knob> (s, ids::revTime, "Time", col::rev);
    kRevMix = std::make_unique<Knob> (s, ids::revMix, "Mix", col::rev);
    for (int i = 0; i < 3; ++i)
    {
        fxLeft[i].setButtonText ("<"); fxRight[i].setButtonText (">");
        fxLeft[i].onClick = [this, i] { moveFx (i, -1); };
        fxRight[i].onClick = [this, i] { moveFx (i, 1); };
        addAndMakeVisible (fxLeft[i]); addAndMakeVisible (fxRight[i]);
    }
    // Invertimos el sentido del knob Crush: a la derecha = más destrucción (hacia 441 Hz)
    kCrush->slider.setRotaryParameters (juce::MathConstants<float>::pi * 2.75f, juce::MathConstants<float>::pi * 1.25f, true);

    for (auto* k : { kIn.get(), kMix.get(), kOut.get(), kSynthMix.get(), kInt1.get(), kInt2.get(), kGain.get(), kDepth.get(),
                     kModMix.get(), kCrush.get(), kBits.get(), kRingF.get(), kRingMix.get(), kTime.get(), kRevMix.get() })
        addAndMakeVisible (k);

    versionLabel.setText (u8 ("FuzzHead · v1.0.1 · Sampled Head"), juce::dontSendNotification);
    versionLabel.setColour (juce::Label::textColourId, col::cream.withAlpha (0.8f));
    versionLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (versionLabel);

    refreshPresets();
    setSize (980, 630);
    timerCallback();
    startTimerHz (15);
}

FuzzHeadEditor::~FuzzHeadEditor() { setLookAndFeel (nullptr); }

void FuzzHeadEditor::setParam (const juce::String& id, float v)
{
    if (auto* r = proc.apvts.getParameter (id))
    {
        r->beginChangeGesture();
        r->setValueNotifyingHost (r->convertTo0to1 (v));
        r->endChangeGesture();
    }
}

void FuzzHeadEditor::refreshPresets()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& f = factoryPresets();
    presetBox.addSectionHeading (u8 ("Fábrica"));
    for (int i = 0; i < (int) f.size(); ++i) presetBox.addItem (u8 (f[(size_t) i].name), i + 1);
    const auto user = proc.getUserPresetNames();
    if (! user.isEmpty())
    {
        presetBox.addSectionHeading ("Usuario");
        for (int i = 0; i < user.size(); ++i) presetBox.addItem (user[i], 1000 + i);
    }
    presetCount = presetBox.getNumItems();
    // reflejar el preset actual
    for (int i = 0; i < presetBox.getNumItems(); ++i)
        if (presetBox.getItemText (i) == proc.currentPresetName) { presetBox.setSelectedItemIndex (i, juce::dontSendNotification); break; }
    lastPresetName = proc.currentPresetName;
}

void FuzzHeadEditor::selectPreset (int itemId)
{
    if (itemId <= 0) return;
    if (itemId < 1000) proc.loadFactory (itemId - 1);
    else proc.loadUserPreset (presetBox.getItemText (presetBox.getSelectedItemIndex()));
    lastPresetName = proc.currentPresetName;
}

void FuzzHeadEditor::moveFx (int fx, int dir)
{
    const int cur = juce::jlimit (0, 5, juce::roundToInt (getParam (ids::order)));
    int o[3] = { kOrders[cur][0], kOrders[cur][1], kOrders[cur][2] };
    int pos = 0; for (int k = 0; k < 3; ++k) if (o[k] == fx) pos = k;
    const int np = pos + dir;
    if (np < 0 || np > 2) return;
    std::swap (o[pos], o[np]);
    for (int i = 0; i < 6; ++i)
        if (kOrders[i][0] == o[0] && kOrders[i][1] == o[1] && kOrders[i][2] == o[2]) { setParam (ids::order, (float) i); break; }
}

void FuzzHeadEditor::timerCallback()
{
    const int eng = juce::roundToInt (getParam (ids::engine));
    for (int i = 0; i < 5; ++i) engineButtons[i]->setToggleState (i == eng, juce::dontSendNotification);
    const int mode = juce::roundToInt (getParam (ids::distMode));
    for (int i = 0; i < 4; ++i) modeButtons[i]->setToggleState (i == mode, juce::dontSendNotification);
    preB.setToggleState (getParam (ids::distPre) > 0.5f, juce::dontSendNotification);
    postB.setToggleState (getParam (ids::distPost) > 0.5f, juce::dontSendNotification);
    const bool bass = getParam (ids::instrument) > 0.5f;
    instGuitar.setToggleState (! bass, juce::dontSendNotification);
    instBass.setToggleState (bass, juce::dontSendNotification);
    engineDesc.setText (u8 (kEngineDesc[juce::jlimit (0, 4, eng)]), juce::dontSendNotification);

    const bool harm = eng == 4;
    kInt1->setVisible (harm); kInt2->setVisible (harm); swInt2->setVisible (harm); int2Label.setVisible (harm);
    kInt2->setAlpha (getParam (ids::int2On) > 0.5f ? 1.0f : 0.35f);

    auto dim = [] (std::initializer_list<juce::Component*> cs, bool on) { for (auto* c : cs) c->setAlpha (on ? 1.0f : 0.4f); };
    const bool synthOn = getParam (ids::synthOn) > 0.5f;
    dim ({ kSynthMix.get(), kInt1.get(), &engineDesc }, synthOn);
    dim ({ kGain.get() }, getParam (ids::distOn) > 0.5f);
    dim ({ kDepth.get(), kModMix.get() }, getParam (ids::modOn) > 0.5f && synthOn);
    dim ({ kCrush.get(), kBits.get() }, getParam (ids::crushOn) > 0.5f);
    dim ({ kRingF.get(), kRingMix.get() }, getParam (ids::ringOn) > 0.5f);
    dim ({ kTime.get(), kRevMix.get() }, getParam (ids::revOn) > 0.5f);

    const int order = juce::roundToInt (getParam (ids::order));
    if (order != lastOrder || eng != lastEngine) { lastOrder = order; lastEngine = eng; resized(); }
    if (proc.currentPresetName != lastPresetName) refreshPresets();
}

void FuzzHeadEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    // gran círculo naranja detrás del logotipo (como en el logo)
    g.setColour (col::accent);
    g.fillEllipse (-40.0f, -210.0f, 360.0f, 330.0f);

    // icono: placa de sampler sobre círculo
    {
        const float cx = 46.0f, cy = 50.0f;
        g.setColour (col::orangeD); g.fillEllipse (cx - 28, cy - 26, 56, 56);
        g.setColour (col::accent);  g.fillEllipse (cx - 28, cy - 28, 56, 56);
        g.setColour (col::cream);   g.fillRect (cx - 13.0f, cy - 23.0f, 26.0f, 40.0f);
        g.setColour (col::accent);
        for (int i = 0; i < 4; ++i) g.fillEllipse (cx - 10 + i * 6.0f, cy - 20, 3.5f, 3.5f);
        g.fillEllipse (cx - 4.5f, cy - 14, 9, 9);
        for (int rr = 0; rr < 3; ++rr) for (int c = 0; c < 4; ++c) g.fillRect (cx - 11 + c * 5.8f, cy - 2 + rr * 6.0f, 4.6f, 4.6f);
        g.setColour (col::cream); g.fillEllipse (cx - 2, cy - 11.5f, 4, 4);
    }
    // palabra FUZZHEAD: relleno negro, contorno crema y sombra (estilo grafiti)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (juce::Font (juce::FontOptions (40.0f, juce::Font::bold | juce::Font::italic)), "FUZZHEAD", 0, 0);
        juce::Path p; ga.createPath (p);
        const auto t = juce::AffineTransform::rotation (-0.05f).translated (86.0f, 66.0f);
        p.applyTransform (t);
        g.setColour (juce::Colour (0xff2b2622)); g.fillPath (p, juce::AffineTransform::translation (4.0f, 4.0f));
        g.setColour (col::cream); g.strokePath (p, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col::text); g.fillPath (p);
        // goteos
        for (auto d : { std::pair<float,float> { 104.0f, 14.0f }, { 152.0f, 20.0f }, { 205.0f, 11.0f }, { 252.0f, 17.0f } })
        {
            const float yy = 66.0f - (d.first - 86.0f) * 0.05f;
            g.setColour (col::cream); g.fillRoundedRectangle (d.first - 3.0f, yy - 2.0f, 7.0f, d.second + 3.0f, 3.5f);
            g.setColour (col::text);  g.fillRoundedRectangle (d.first - 1.5f, yy - 2.0f, 4.0f, d.second, 2.0f);
        }
    }
    g.setColour (col::text); g.fillRoundedRectangle (86.0f, 92.0f, 172.0f, 18.0f, 2.0f);
    g.setColour (col::cream); g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("GUITAR & BASS SYNTH", 86, 92, 172, 18, juce::Justification::centred);

    // línea de la cadena
    const int order = juce::jlimit (0, 5, juce::roundToInt (getParam (ids::order)));
    const char* nm[] = { "Bitcrush", "Ring Mod", "Gated Reverb" };
    juce::String chain = "Input > Sinte(+Mod) > Dist PRE > ";
    for (int k = 0; k < 3; ++k) chain << nm[kOrders[order][k]] << " > ";
    chain << "Dist POST > Mix > Output";
    g.setColour (col::cream.withAlpha (0.7f));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (chain, 18, 362, 900, 16, juce::Justification::centredLeft);
}

void FuzzHeadEditor::resized()
{
    const int kw = 82, kh = 98;
    // Barra superior
    instGuitar.setBounds (290, 20, 84, 28); instBass.setBounds (378, 20, 64, 28);
    presetBox.setBounds (290, 60, 230, 28);
    prevB.setBounds (526, 60, 30, 28); nextB.setBounds (560, 60, 30, 28);
    saveB.setBounds (596, 60, 70, 28); delB.setBounds (670, 60, 64, 28);
    kIn->setBounds (744, 6, 78, kh); kMix->setBounds (822, 6, 78, kh); kOut->setBounds (900, 6, 78, kh);

    // Fila 1
    const int y1 = 116, h1 = 240;
    pSynth.setBounds (16, y1, 430, h1);
    pDist.setBounds (456, y1, 280, h1);
    pMod.setBounds (746, y1, 218, h1);
    swSynth->setBounds (pSynth.getRight() - 54, y1 + 10, 46, 26);
    for (int i = 0; i < 5; ++i) engineButtons[i]->setBounds (30 + i * 70, y1 + 44, 66, 28);
    engineDesc.setBounds (26, y1 + 76, 400, 20);
    kSynthMix->setBounds (pSynth.getRight() - kw - 8, y1 + 90, kw, kh + 10);
    kInt1->setBounds (30, y1 + 104, kw, kh + 10); kInt2->setBounds (30 + kw + 8, y1 + 104, kw, kh + 10);
    swInt2->setBounds (30 + 2 * kw + 22, y1 + 140, 46, 26); int2Label.setBounds (30 + 2 * kw + 70, y1 + 140, 90, 26);

    swDist->setBounds (pDist.getRight() - 54, y1 + 10, 46, 26);
    kGain->setBounds (pDist.getX() + 12, y1 + 44, kw, kh + 10);
    for (int i = 0; i < 4; ++i) modeButtons[i]->setBounds (pDist.getX() + 106 + (i % 2) * 84, y1 + 48 + (i / 2) * 34, 80, 28);
    preB.setBounds (pDist.getX() + 106, y1 + 128, 80, 28); postB.setBounds (pDist.getX() + 190, y1 + 128, 80, 28);

    swMod->setBounds (pMod.getRight() - 54, y1 + 10, 46, 26);
    kDepth->setBounds (pMod.getX() + 16, y1 + 60, kw, kh + 10); kModMix->setBounds (pMod.getX() + 120, y1 + 60, kw, kh + 10);

    // Fila 2: orden según el parámetro
    const int y2 = 384, h2 = 200, w2 = 308;
    const int order = juce::jlimit (0, 5, juce::roundToInt (getParam (ids::order)));
    ModulePanel* panels[3] = { &pCrush, &pRing, &pRev };
    juce::ToggleButton* sws[3] = { swCrush.get(), swRing.get(), swRev.get() };
    Knob* ka[3] = { kCrush.get(), kRingF.get(), kTime.get() };
    Knob* kb[3] = { kBits.get(), kRingMix.get(), kRevMix.get() };
    for (int slot = 0; slot < 3; ++slot)
    {
        const int fx = kOrders[order][slot];
        const int x = 16 + slot * (w2 + 12);
        panels[fx]->setBounds (x, y2, w2, h2);
        sws[fx]->setBounds (x + w2 - 54, y2 + 10, 46, 26);
        fxLeft[fx].setBounds (x + w2 - 120, y2 + 12, 26, 22);
        fxRight[fx].setBounds (x + w2 - 90, y2 + 12, 26, 22);
        ka[fx]->setBounds (x + 50, y2 + 60, kw, kh + 10);
        kb[fx]->setBounds (x + w2 - 50 - kw, y2 + 60, kw, kh + 10);
        fxLeft[fx].setEnabled (slot > 0); fxRight[fx].setEnabled (slot < 2);
    }
    versionLabel.setBounds (16, getHeight() - 30, 300, 20);
    repaint();
}
