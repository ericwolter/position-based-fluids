#pragma once

#include <list>
#include <string>
using namespace std;

const string getPathForScenario(const string scenario);
const string getPathForKernel  (const string kernel);
const string getPathForShader  (const string shader);
const string getPathForTexture (const string texture);
const string getRootPath();

bool DetectResourceChanges(list<pair<string, time_t> >& fileList);
