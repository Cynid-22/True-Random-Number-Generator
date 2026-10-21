#pragma once
#include "../entropy/entropy_common.h"

void RenderSampleExportMenu();
void RenderSampleExportWindow();

bool UpdateSampleExport();
bool RecordSampleExportData(const Entropy::EntropyDataVector &data);
