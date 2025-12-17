#include "controller.h"
#include "pluginids.h"
#include "pluginterfaces/vst/vsttypes.h"
#include "processor.h"
#include "public.sdk/source/main/pluginfactory.h"

using namespace Steinberg;

#define stringCompanyName "WineInfo"
#define stringPluginName "WineInfo"
#define stringPluginCategory "Fx"
#define stringPluginVersion "1.0.0"

BEGIN_FACTORY_DEF(stringCompanyName, "https://example.invalid",
                  "mailto:example@example.invalid")

DEF_CLASS2(INLINE_UID_FROM_FUID(WineInfo::kProcessorUID),
           PClassInfo::kManyInstances, kVstAudioEffectClass, stringPluginName,
           Vst::kDistributable, stringPluginCategory, stringPluginVersion,
           kVstVersionString, WineInfo::Processor::createInstance)

DEF_CLASS2(INLINE_UID_FROM_FUID(WineInfo::kControllerUID),
           PClassInfo::kManyInstances, kVstComponentControllerClass,
           stringPluginName " Controller", 0, "", stringPluginVersion,
           kVstVersionString, WineInfo::Controller::createInstance)

END_FACTORY
