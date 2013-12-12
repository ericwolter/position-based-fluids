#pragma once

struct Parameters
{
	// Runner related
	int  resetSimOnChange;

	// Scene related
	unsigned int  particleCount;
	float xMin;
	float xMax;
	float yMin;
	float yMax;
	float zMin;
	float zMax;

	float waveGenAmp;
	float waveGenFreq;
	float waveGenDuty;

	// Simulation consts
	float timeStep;
	unsigned int   simIterations;
	unsigned int   subSteps;
	float h;
	float restDensity;
	float epsilon;
	float garvity;
	float vorticityFactor;
	float viscosityFactor;
	float surfaceTenstionK;
	float surfaceTenstionDist;

	// Grid and friends list
	unsigned int  friendsCircles;
	unsigned int  particlesPerCircle;
	unsigned int  gridBufSize;

	// Setup related
	float setupSpacing;

	// Sorting
	unsigned int segmentSize;
	unsigned int sortIterations;

	// Rendering related
	float particleRenderSize;

	// Computed fields
	float h_2;
};
