#pragma once

#include <list>
#include <string>
using namespace std;

const string getPathForScenario(const string scenario);
const string getPathForKernel  (const string kernel);
const string getPathForShader  (const string shader);
const string getPathForTexture (const string texture);
const string getPathForObjects (const string object);
const string getRootPath();

const string getScenario       (const string name);
const string getShaderSource   (const string shader);
const string getKernelSource   (const string kernel);

bool DetectResourceChanges(list<pair<string, time_t> >& fileList);
