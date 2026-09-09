/***************************************************************************
 * Concrete azimuthal projections + plugin entry point.
 *
 *   "Orthographic"          - globe seen from infinity (one hemisphere)
 *   "Stereographic"         - conformal globe (one hemisphere)
 *   "AzimuthalEquidistant"  - whole Earth, true distances from the centre
 *
 * Copyright (C) 2025 Mustafa Comoglu
 *
 * GNU Affero General Public License version 3.0; see the LICENSE file.
 ***************************************************************************/


#include "azimuthal.h"

#include <seiscomp/core/plugin.h>

#include <cmath>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


namespace Seiscomp {
namespace Gui {
namespace Map {


// ======================================================================
//  Orthographic:  rho = sin c,  c <= 90 deg
// ======================================================================
class OrthographicProjection : public AzimuthalProjection {
	protected:
		double radius(double, double sinc, double) const override {
			return sinc;
		}
		double distance(double rho) const override {
			return std::asin(rho > 1.0 ? 1.0 : rho < -1.0 ? -1.0 : rho);
		}
		double limbRadius() const override { return 1.0; }
		double cMax() const override       { return M_PI / 2.0; }
};

REGISTER_PROJECTION_INTERFACE(OrthographicProjection, "Orthographic");


// ======================================================================
//  Stereographic:  rho = 2 tan(c/2),  clipped to c <= 90 deg
// ======================================================================
class StereographicProjection : public AzimuthalProjection {
	protected:
		double radius(double, double sinc, double cosc) const override {
			const double d = 1.0 + cosc;
			return d > 1.0e-12 ? 2.0 * sinc / d : 2.0e12;
		}
		double distance(double rho) const override {
			return 2.0 * std::atan(rho * 0.5);
		}
		double limbRadius() const override { return 2.0; }   // 2 tan(45 deg)
		double cMax() const override       { return M_PI / 2.0; }
};

REGISTER_PROJECTION_INTERFACE(StereographicProjection, "Stereographic");


// ======================================================================
//  Azimuthal equidistant:  rho = c,  c <= 180 deg (whole Earth)
// ======================================================================
class AzimuthalEquidistantProjection : public AzimuthalProjection {
	protected:
		double radius(double c, double, double) const override {
			return c;
		}
		double distance(double rho) const override {
			return rho > M_PI ? M_PI : rho;
		}
		double limbRadius() const override { return M_PI; }
		double cMax() const override       { return M_PI; }
};

REGISTER_PROJECTION_INTERFACE(AzimuthalEquidistantProjection,
                              "AzimuthalEquidistant");


// ======================================================================
//  Lambert azimuthal equal-area:  rho = 2 sin(c/2) = sqrt(2 (1 - cos c)),
//  c <= 180 deg (whole Earth, areas preserved everywhere)
// ======================================================================
class LambertAzimuthalEqualAreaProjection : public AzimuthalProjection {
	protected:
		double radius(double, double, double cosc) const override {
			double d = 2.0 * (1.0 - cosc);
			return d > 0.0 ? std::sqrt(d) : 0.0;
		}
		double distance(double rho) const override {
			double h = rho * 0.5;
			return 2.0 * std::asin(h > 1.0 ? 1.0 : h);
		}
		double limbRadius() const override { return 2.0; }   // 2 sin(90 deg)
		double cMax() const override       { return M_PI; }
};

REGISTER_PROJECTION_INTERFACE(LambertAzimuthalEqualAreaProjection,
                              "LambertAzimuthalEqualArea");


}
}
}


ADD_SC_PLUGIN(
	"Spherical azimuthal map projections for the SeisComP GUI: "
	"Orthographic, Stereographic, AzimuthalEquidistant and "
	"LambertAzimuthalEqualArea",
	"Mustafa Comoglu",
	1, 0, 0
)
