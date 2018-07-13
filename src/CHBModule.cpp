
#include <sstream>
#include "Squinky.hpp"
#include "WidgetComposite.h"


#include "CHB.h"


/**
 */
struct CHBModule : Module
{
public:
    CHBModule();
    /**
     *
     * Overrides of Module functions
     */
    void step() override;
    void onSampleRateChange() override;

    CHB<WidgetComposite> chb;
private:
};

void CHBModule::onSampleRateChange()
{
    //float rate = engineGetSampleRate();
   // gmr.setSampleRate(rate);
   float sampleTime = engineGetSampleTime();
   chb.setSampleTime(sampleTime);
}

CHBModule::CHBModule()
    : Module(chb.NUM_PARAMS,
    chb.NUM_INPUTS,
    chb.NUM_OUTPUTS,
    chb.NUM_LIGHTS),
    chb(this)
{
    onSampleRateChange();
    chb.init();
}

void CHBModule::step()
{
    chb.step();
}

////////////////////
// module widget
////////////////////

struct CHBWidget : ModuleWidget
{
    CHBWidget(CHBModule *);

    void addLabel(const Vec& v, const char* str, const NVGcolor& color = COLOR_BLACK)
    {
        Label* label = new Label();
        label->box.pos = v;
        label->text = str;
        label->color = color;
        addChild(label);
    }

    void addHarmonics(CHBModule *module, const Vec& pos);
    void addHarmonicsRow(CHBModule *module, int row, const Vec& pos);
};


/*
    const float knobX = 25;
    const float knobY= 70;
    const float knobDY = 45;
    */
inline void CHBWidget::addHarmonics(CHBModule *module, const Vec& pos)
{
    addHarmonicsRow(module, 0, pos);
    Vec pos2 = pos;
    pos2.y += 40;
    addHarmonicsRow(module, 1, pos2);
}
inline void CHBWidget::addHarmonicsRow(CHBModule *module, int row, const Vec& pos)
{
    int firstParam = 0;
    int lastParam = 0;
    switch (row) {
        case 0:
            firstParam = module->chb.PARAM_H0;
            lastParam = module->chb.PARAM_H5;
            break;
        case 1:
            firstParam = module->chb.PARAM_H6;
            lastParam = module->chb.PARAM_H10;
            break;
        default:
            assert(false);
    }
   
  //  printf("%d, %d, %d\n", row, firstParam, lastParam); fflush(stdout);
   // return;
    for (int param = firstParam; param <= lastParam; ++param) {
        Vec p;
        p.x = pos.x + (param - firstParam) * 30;
        p.y = pos.y;

        printf("create trimpot %d\n", param); fflush(stdout);
        addParam(ParamWidget::create<Trimpot>(
            p, module, param, 0.0f, 1.0f, 1.0f));
    }
}

/**
 * Widget constructor will describe my implementation structure and
 * provide meta-data.
 * This is not shared by all modules in the DLL, just one
 */
CHBWidget::CHBWidget(CHBModule *module) : ModuleWidget(module)
{ 
    box.size = Vec(16 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
    {
        SVGPanel *panel = new SVGPanel();
        panel->box.size = box.size;
        panel->setBackground(SVG::load(assetPlugin(plugin, "res/chb_panel.svg")));
        addChild(panel);
    }
    

    const float row1=30;
    const float label1 = row1+30;
    addInput(Port::create<PJ301MPort>(
        Vec(20, row1), Port::INPUT, module, module->chb.CV_INPUT));
    addLabel(Vec(15, label1), "CV");

    addInput(Port::create<PJ301MPort>(
        Vec(70, row1), Port::INPUT, module, module->chb.ENV_INPUT));
    addLabel(Vec(65, label1), "EG");
 

    addParam(ParamWidget::create<Trimpot>(
        Vec(150, 100), module, module->chb.PARAM_PITCH, -5.0f, 5.0f, 0));
    addLabel(Vec(140, 120), "Pitch");



    addHarmonics(module, Vec(25, 220));

    addOutput(Port::create<PJ301MPort>(
        Vec(40, 300), Port::OUTPUT, module, module->chb.OUTPUT));
    addLabel(Vec(35, 320), "Out");
 


    // screws
    addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}

Model *modelCHBModule = Model::create<CHBModule,
    CHBWidget>("Squinky Labs",
    "squinkylabs-CHB",
    "CHB", EFFECT_TAG, LFO_TAG);


