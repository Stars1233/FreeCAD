/***************************************************************************
 *   Copyright (c) 2015 FreeCAD Developers                                 *
 *   Authors: Michael Hindley <hindlemp@eskom.co.za>                       *
 *            Ruan Olwagen <olwager@eskom.co.za>                           *
 *            Oswald van Ginkel <vginkeo@eskom.co.za>                      *
 *   Based on Force constraint by Jan Rheinländer                          *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"

#include "FemConstraintTemperature.h"


using namespace Fem;

PROPERTY_SOURCE(Fem::ConstraintTemperature, Fem::Constraint)

static const char* ConstraintTypes[] = {"CFlux", "Temperature", nullptr};

ConstraintTemperature::ConstraintTemperature()
{
    ADD_PROPERTY(Temperature, (300.0));
    ADD_PROPERTY(CFlux, (0.0));
    ADD_PROPERTY_TYPE(ConstraintType,
                      (1),
                      "ConstraintTemperature",
                      (App::PropertyType)(App::Prop_None),
                      "Type of constraint, temperature or concentrated heat flux");
    ConstraintType.setEnums(ConstraintTypes);
    ADD_PROPERTY_TYPE(EnableAmplitude,
                      (false),
                      "ConstraintTemperature",
                      (App::PropertyType)(App::Prop_None),
                      "Amplitude of the temperature boundary condition");
    ADD_PROPERTY_TYPE(AmplitudeValues,
                      (std::vector<std::string> {"0, 0", "1, 1"}),
                      "ConstraintTemperature",
                      (App::PropertyType)(App::Prop_None),
                      "Amplitude values");
}

App::DocumentObjectExecReturn* ConstraintTemperature::execute()
{
    return Constraint::execute();
}

const char* ConstraintTemperature::getViewProviderName() const
{
    return "FemGui::ViewProviderFemConstraintTemperature";
}

void ConstraintTemperature::handleChangedPropertyType(Base::XMLReader& reader,
                                                      const char* TypeName,
                                                      App::Property* prop)
{
    // property Temperature had App::PropertyFloat and was changed to App::PropertyTemperature
    if (prop == &Temperature && strcmp(TypeName, "App::PropertyFloat") == 0) {
        App::PropertyFloat TemperatureProperty;
        // restore the PropertyFloat to be able to set its value
        TemperatureProperty.Restore(reader);
        Temperature.setValue(TemperatureProperty.getValue());
    }
    // property CFlux had App::PropertyFloat and was changed to App::PropertyPower
    else if (prop == &CFlux && strcmp(TypeName, "App::PropertyFloat") == 0) {
        App::PropertyFloat CFluxProperty;
        CFluxProperty.Restore(reader);
        CFlux.setValue(CFluxProperty.getValue());
    }
    else {
        Constraint::handleChangedPropertyType(reader, TypeName, prop);
    }
}

void ConstraintTemperature::onChanged(const App::Property* prop)
{
    Constraint::onChanged(prop);
}
