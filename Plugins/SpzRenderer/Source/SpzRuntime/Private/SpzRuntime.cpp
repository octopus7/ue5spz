#include "SpzRuntime.h"

#define LOCTEXT_NAMESPACE "FSpzRuntimeModule"

DEFINE_LOG_CATEGORY(LogSpzRuntime);

void FSpzRuntimeModule::StartupModule()
{
}

void FSpzRuntimeModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSpzRuntimeModule, SpzRuntime)
