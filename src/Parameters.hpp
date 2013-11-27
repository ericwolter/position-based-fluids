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
	float restDensity;
	float epsilon;
	float garvity;
	float vorticityFactor;
	float viscosityFactor;
	float surfaceTenstionK;
	float surfaceTenstionDist;

	// Grid and friends list
	int  friendsCircles;
	int  particlesPerCircle;
	int  gridRes;

	// Setup related
	float setupSpacing;

	// Sorting
	unsigned int segmentSize;
	unsigned int sortIterations;

	// Computed fields
	float h;
	float h_2;
};
