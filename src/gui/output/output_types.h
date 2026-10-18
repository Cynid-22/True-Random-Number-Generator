#pragma once

struct OutputType {
  const char *name;
  int format;
  int rawDataEncoding;
};

struct OutputCategory {
  const char *name;
  const OutputType *types;
  int typeCount;
};

int GetOutputCategoryCount();

const OutputCategory &GetOutputCategory(int category);

void FindCurrentOutputType(int &category, int &type);

void SelectOutputType(int category, int type);

void SelectOutputCategory(int category);

void RememberOutputType(int category, int type);
