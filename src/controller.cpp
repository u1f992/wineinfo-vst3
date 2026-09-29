#include "controller.h"
#include "pluginids.h"
#include "view_vulkan.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace WineInfo {

static void addInfoParam(ParameterContainer &params, ParamID id,
                         const wchar_t *label, const std::wstring &value) {
  std::wstring title = std::wstring(label) + value;

  String128 title128;
  size_t len = title.length();
  if (len > 127)
    len = 127;
  for (size_t i = 0; i < len; ++i) {
    title128[i] = static_cast<char16_t>(title[i]);
  }
  title128[len] = 0;

  params.addParameter(title128, nullptr, 0, 0, ParameterInfo::kIsReadOnly, id);
}

tresult PLUGIN_API Controller::initialize(FUnknown *context) {
  tresult r = EditControllerEx1::initialize(context);
  if (r != kResultOk)
    return r;

  info_ = get_info_strings();

  addInfoParam(parameters, kParamVersion, L"Wine version: ", info_.version);
  addInfoParam(parameters, kParamBuildId, L"Wine build-id: ", info_.build_id);
  addInfoParam(parameters, kParamProcSelfExe, L"/proc/self/exe: ",
               info_.proc_self_exe);
  addInfoParam(parameters, kParamWineloader, L"WINELOADER: ", info_.wineloader);
  addInfoParam(parameters, kParamWineprefix, L"WINEPREFIX: ", info_.wineprefix);
  addInfoParam(parameters, kParamWineserver, L"WINESERVER: ", info_.wineserver);

  return kResultOk;
}

IPlugView *PLUGIN_API Controller::createView(FIDString name) {
  if (FIDStringsEqual(name, ViewType::kEditor))
    return new ViewVulkan(info_);
  return nullptr;
}

} // namespace WineInfo
