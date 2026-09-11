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


namespace Seiscomp::Gui::Map {


namespace {

const double PI = 3.14159265358979323846;

}


// ======================================================================
//  Orthographic:  rho = sin c,  c <= 90 deg
// ======================================================================
class OrthographicProjection : public AzimuthalProjection {
	protected:
		double radius(double c, double sinc, double cosc) const override {
			(void)c;
			(void)cosc;
			return sinc;
		}
		void sinCos(double rho, double rho2,
		            double &sinc, double &cosc) const override {
			// rho = sin c
			sinc = rho > 1.0 ? 1.0 : rho;
			const double c2 = 1.0 - rho2;
			cosc = c2 > 0.0 ? std::sqrt(c2) : 0.0;
		}
		double limbRadius() const override {
			return 1.0;
		}
		double cMax() const override {
			return PI / 2.0;
		}
};

REGISTER_PROJECTION_INTERFACE(OrthographicProjection, "Orthographic");


// ======================================================================
//  Stereographic:  rho = 2 tan(c/2),  clipped to c <= 90 deg
// ======================================================================
class StereographicProjection : public AzimuthalProjection {
	protected:
		double radius(double c, double sinc, double cosc) const override {
			(void)c;
			const double d = 1.0 + cosc;
			return d > 1.0e-12 ? 2.0 * sinc / d : 2.0e12;
		}
		void sinCos(double rho, double rho2,
		            double &sinc, double &cosc) const override {
			// rho = 2 tan(c/2)  ->  half-angle identities, no trig
			const double t = rho2 * 0.25;   // tan^2(c/2)
			const double d = 1.0 + t;
			sinc = rho / d;
			cosc = (1.0 - t) / d;
		}
		double limbRadius() const override {
			return 2.0;                     // 2 tan(45 deg)
		}
		double cMax() const override {
			return PI / 2.0;
		}
};

REGISTER_PROJECTION_INTERFACE(StereographicProjection, "Stereographic");


// ======================================================================
//  Azimuthal equidistant:  rho = c,  c <= 180 deg (whole Earth)
// ======================================================================
class AzimuthalEquidistantProjection : public AzimuthalProjection {
	protected:
		double radius(double c, double sinc, double cosc) const override {
			(void)sinc;
			(void)cosc;
			return c;
		}
		void sinCos(double rho, double rho2,
		            double &sinc, double &cosc) const override {
			// c = rho
			(void)rho2;
			sinc = std::sin(rho);
			cosc = std::cos(rho);
		}
		double limbRadius() const override {
			return PI;
		}
		double cMax() const override {
			return PI;
		}
};

REGISTER_PROJECTION_INTERFACE(AzimuthalEquidistantProjection,
                              "AzimuthalEquidistant");


// ======================================================================
//  Lambert azimuthal equal-area:  rho = 2 sin(c/2) = sqrt(2 (1 - cos c)),
//  c <= 180 deg (whole Earth, areas preserved everywhere)
// ======================================================================
class LambertAzimuthalEqualAreaProjection : public AzimuthalProjection {
	protected:
		double radius(double c, double sinc, double cosc) const override {
			(void)c;
			(void)sinc;
			const double d = 2.0 * (1.0 - cosc);
			return d > 0.0 ? std::sqrt(d) : 0.0;
		}
		void sinCos(double rho, double rho2,
		            double &sinc, double &cosc) const override {
			// rho = 2 sin(c/2)  ->  double-angle identities, one sqrt
			cosc = 1.0 - rho2 * 0.5;
			const double hh = 1.0 - rho2 * 0.25;   // cos^2(c/2)
			sinc = rho * (hh > 0.0 ? std::sqrt(hh) : 0.0);
		}
		double limbRadius() const override {
			return 2.0;                            // 2 sin(90 deg)
		}
		double cMax() const override {
			return PI;
		}
};

REGISTER_PROJECTION_INTERFACE(LambertAzimuthalEqualAreaProjection,
                              "LambertAzimuthalEqualArea");


}


ADD_SC_PLUGIN(
	"Spherical azimuthal map projections for the SeisComP GUI: "
	"Orthographic, Stereographic, AzimuthalEquidistant and "
	"LambertAzimuthalEqualArea",
	"Mustafa Comoglu",
	1, 0, 0
)
