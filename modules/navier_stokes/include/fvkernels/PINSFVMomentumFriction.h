//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "FVElementalKernel.h"

/**
 * Imposes a friction force on the momentum equation in porous media.
 */
class PINSFVMomentumFriction : public FVElementalKernel
{
public:
  static InputParameters validParams();
  PINSFVMomentumFriction(const InputParameters & params);

protected:
  ADReal computeQpResidual() override;

  /// Momentum equation component (x = 0, y = 1, z = 2)
  const unsigned int _index;
  /// Darcy coefficient
  const Moose::Functor<ADRealVectorValue> * const _cL;
  /// Forchheimer coefficient
  const Moose::Functor<ADRealVectorValue> * const _cQ;
  /// Booleans to select the right models
  const bool _use_Darcy_friction_model;
  const bool _use_Forchheimer_friction_model;
  /// Porosity to compute the intersitial velocity from the superficial velocity
  const Moose::Functor<ADReal> & _eps;
  /// Density as a functor
  const Moose::Functor<ADReal> & _rho;
};
