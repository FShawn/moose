//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "VolumetricFlowRate.h"
#include "MathFVUtils.h"
#include <math.h>

registerMooseObject("NavierStokesApp", VolumetricFlowRate);

InputParameters
VolumetricFlowRate::validParams()
{
  InputParameters params = SideIntegralPostprocessor::validParams();
  params.addClassDescription(
      "Computes the volumetric flow rate of an advected quantity through an external boundary.");
  params.addParam<bool>("fv", false, "Whether finite volume variables are used");
  params.addRequiredCoupledVar("vel_x", "The x-axis velocity");
  params.addCoupledVar("vel_y", 0, "The y-axis velocity");
  params.addCoupledVar("vel_z", 0, "The z-axis velocity");
  params.addCoupledVar(
      "advected_variable", 0, "The advected variable quantity of which to study the flow");
  params.addParam<MooseFunctorName>(
      "advected_mat_prop", 0, "The advected material property of which to study the flow");
  MooseEnum advected_interp_method("average upwind", "upwind");
  params.addParam<MooseEnum>("advected_interp_method",
                             advected_interp_method,
                             "The interpolation to use for the advected quantity. Options are "
                             "'upwind' and 'average', with the default being 'upwind'.");
  return params;
}

VolumetricFlowRate::VolumetricFlowRate(const InputParameters & parameters)
  : SideIntegralPostprocessor(parameters),
    _fv(getParam<bool>("fv")),
    _vel_x(coupledValue("vel_x")),
    _vel_y(coupledValue("vel_y")),
    _vel_z(coupledValue("vel_z")),
    _fv_vel_x(dynamic_cast<const MooseVariableFV<Real> *>(getFieldVar("vel_x", 0))),
    _fv_vel_y(dynamic_cast<const MooseVariableFV<Real> *>(getFieldVar("vel_y", 0))),
    _fv_vel_z(dynamic_cast<const MooseVariableFV<Real> *>(getFieldVar("vel_z", 0))),
    _advected_variable_supplied(parameters.isParamSetByUser("advected_variable")),
    _advected_variable(coupledValue("advected_variable")),
    _fv_advected_variable(
        dynamic_cast<const MooseVariableFV<Real> *>(getFieldVar("advected_variable", 0))),
    _advected_mat_prop_supplied(parameters.isParamSetByUser("advected_mat_prop")),
    _advected_material_property(getFunctor<ADReal>("advected_mat_prop"))
{
  // Check that at most one advected quantity has been provided
  if (_advected_variable_supplied && _advected_mat_prop_supplied)
    mooseError("VolumetricFlowRatePostprocessor should be provided either an advected variable "
               "or an advected material property");

  // Check that the user isn't trying to get face values for material properties
  if (parameters.isParamSetByUser("advected_interp_method") && _advected_mat_prop_supplied)
    mooseWarning("Advected quantity interpolation methods are currently unavailable for "
                 "advected material properties.");

  using namespace Moose::FV;

  const auto & advected_interp_method = getParam<MooseEnum>("advected_interp_method");
  if (advected_interp_method == "average")
    _advected_interp_method = InterpMethod::Average;
  else if (advected_interp_method == "upwind")
    _advected_interp_method = InterpMethod::Upwind;
  else
    mooseError("Unrecognized advected quantity interpolation type ",
               static_cast<std::string>(advected_interp_method));
}

Real
VolumetricFlowRate::computeQpIntegral()
{
#ifdef MOOSE_GLOBAL_AD_INDEXING
  if (_fv)
  {
    // We should be at the edge of the domain
    const FaceInfo * const fi = _mesh.faceInfo(_current_elem, _current_side);
    mooseAssert(fi, "We should have a face info");

    // Get face value for velocity
    const auto & vx_face =
        _fv_vel_x ? MetaPhysicL::raw_value(_fv_vel_x->getBoundaryFaceValue(*fi)) : _vel_x[_qp];

    const auto & vy_face =
        _fv_vel_y ? MetaPhysicL::raw_value(_fv_vel_y->getBoundaryFaceValue(*fi)) : _vel_y[_qp];

    const auto & vz_face =
        _fv_vel_z ? MetaPhysicL::raw_value(_fv_vel_z->getBoundaryFaceValue(*fi)) : _vel_z[_qp];

    // Compute the advected quantity on the face
    Real advected_quantity;
    if (_advected_variable_supplied)
    {
      // Get neighbor value
      const Elem * const neighbor = _current_elem->neighbor_ptr(_current_side);
      const auto & advected_variable_neighbor =
          _fv_advected_variable ? MetaPhysicL::raw_value(_fv_advected_variable->getNeighborValue(
                                      neighbor, *fi, _advected_variable[_qp]))
                                : _advected_variable[_qp];

      Moose::FV::interpolate(_advected_interp_method,
                             advected_quantity,
                             MetaPhysicL::raw_value(_advected_variable[_qp]),
                             MetaPhysicL::raw_value(advected_variable_neighbor),
                             RealVectorValue(vx_face, vy_face, vz_face),
                             *fi,
                             true);
    }
    else if (_advected_mat_prop_supplied)
      advected_quantity = MetaPhysicL::raw_value(_advected_material_property(_current_elem));
    else
      advected_quantity = 1;

    return advected_quantity * RealVectorValue(vx_face, vy_face, vz_face) * _normals[_qp];
  }
  else
#endif
  {
    if (_advected_variable_supplied)
      return _advected_variable[_qp] * RealVectorValue(_vel_x[_qp], _vel_y[_qp], _vel_z[_qp]) *
             _normals[_qp];
    else if (_advected_mat_prop_supplied)
      return MetaPhysicL::raw_value(
                 _advected_material_property(std::make_tuple(_current_elem, _qp, _qrule))) *
             RealVectorValue(_vel_x[_qp], _vel_y[_qp], _vel_z[_qp]) * _normals[_qp];
    else
      return RealVectorValue(_vel_x[_qp], _vel_y[_qp], _vel_z[_qp]) * _normals[_qp];
  }
}
