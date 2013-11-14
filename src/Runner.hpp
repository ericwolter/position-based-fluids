#ifndef __RUNNER_HPP
#define __RUNNER_HPP

#include "hesp.hpp"
#include "Simulation.hpp"
#include "visual/visual.hpp"


class Runner {
private:
  // Avoid copy
  Runner &operator=(const Runner &other);
  Runner (const Runner &other);

  // A method that checks if things have changed
  bool ResourceChanged() const;

public:
  Runner () {}

  void run(Simulation &simulation, CVisual &renderer) const;

};

#endif // __RUNNER_HPP
