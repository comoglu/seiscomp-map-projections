/***************************************************************************
 * Azimuthal map projections for the SeisComP GUI (libseiscomp_gui).
 *
 * A shared base class implementing the standard spherical azimuthal
 * transforms (Snyder, "Map Projections - A Working Manual", USGS
 * Professional Paper 1395, 1987) plus the SeisComP
 * Seiscomp::Gui::Map::Projection integration. Concrete projections
 * (orthographic, stereographic, azimuthal equidistant) only supply the
 * radial scale function and its inverse.
 *
 * Copyright (C) 2025 Mustafa Comoglu
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License version 3.0 as
 * published by the Free Software Foundation, matching the license of the
 * SeisComP framework it extends. See the LICENSE file for details.
 ***************************************************************************/


#ifndef SEISCOMP_GUI_MAP_PROJECTIONS_AZIMUTHAL_H
#define SEISCOMP_GUI_MAP_PROJECTIONS_AZIMUTHAL_H


#include <QImage>
#include <QPoint>
#include <QPointF>

#include <seiscomp/gui/map/projection.h>


namespace Seiscomp {
namespace Gui {
namespace Map {


/**
 * @brief Common base for spherical azimuthal ("globe") projections.
 *
 * The map is a disc centred on an arbitrary geographic point
 * (the current view centre). A point at angular distance @c c and azimuth
 * @c Az from the centre is drawn at polar radius @c rho(c) and the same
 * azimuth; every concrete projection differs only in @c rho(c):
 *
 *   orthographic          rho = sin c            (c <= 90 deg)
 *   stereographic         rho = 2 tan(c / 2)     (c <= 90 deg here)
 *   azimuthal equidistant rho = c               (c <= 180 deg)
 *
 * Screen coordinates are @c rho normalized by the projection's limb radius
 * so the visible disc always fills the viewport, then scaled by the shared
 * zoom. Pixels outside the disc - and geographic points on the far side of
 * the globe - are reported as not visible / rendered transparent.
 */
class AzimuthalProjection : public Projection {
	// ------------------------------------------------------------------
	//  X'struction
	// ------------------------------------------------------------------
	public:
		AzimuthalProjection();


	// ------------------------------------------------------------------
	//  Per-projection radial functions (implemented by subclasses)
	// ------------------------------------------------------------------
	protected:
		//! Radius rho for a point at angular distance c from the centre.
		//! @p sinc, @p cosc are sin(c), cos(c) (passed to avoid recomputing).
		virtual double radius(double c, double sinc, double cosc) const = 0;

		//! Inverse of radius(): angular distance c for polar radius @p rho.
		virtual double distance(double rho) const = 0;

		//! Polar radius of the visible limb, i.e. radius() at @c cMax().
		virtual double limbRadius() const = 0;

		//! Largest angular distance from the centre that is drawn [rad].
		virtual double cMax() const = 0;


	// ------------------------------------------------------------------
	//  Projection interface
	// ------------------------------------------------------------------
	public:
		bool isRectangular() const override;
		bool wantsGridAntialiasing() const override;

		bool project(QPoint &screenCoords,
		             const QPointF &geoCoords) const override;
		bool unproject(QPointF &geoCoords,
		               const QPoint &screenCoords) const override;

		void centerOn(const QPointF &geoCoords) override;

		int lineSteps(const QPointF &p0, const QPointF &p1) override;

		//! Adds a screen-jump guard on top of the base behaviour so a
		//! poly-line (grid, feature) never draws a chord across the disc
		//! when an edge wraps past the antipode.
		bool lineTo(QPainter &p, const QPointF &to) override;

		void updateBoundingBox() override;

		bool project(QPainterPath &screenPath, size_t n,
		             const Geo::GeoCoordinate *poly, bool closed,
		             uint minPixelDist, ClipHint hint = NoClip) const override;


	// ------------------------------------------------------------------
	//  Rasterization
	// ------------------------------------------------------------------
	protected:
		void render(QImage &img, bool highQuality,
		            TextureCache *cache) override;


	// ------------------------------------------------------------------
	//  Helpers
	// ------------------------------------------------------------------
	private:
		void updateCenter();

		//! Forward transform to normalized disc coordinates (radius <= 1 on
		//! the visible side). Returns false for the far hemisphere / beyond
		//! the limb.
		bool forwardNorm(double lonRad, double latRad,
		                 double &nx, double &ny) const;

		//! Inverse of forwardNorm().
		bool inverseNorm(double nx, double ny,
		                 double &lonRad, double &latRad) const;


	// ------------------------------------------------------------------
	//  Members (view centre, precomputed)
	// ------------------------------------------------------------------
	private:
		double _lam0;      //!< central meridian [rad]
		double _phi1;      //!< central parallel [rad]
		double _sinPhi1;
		double _cosPhi1;
		double _ooLimb;    //!< 1 / limbRadius()
};


}
}
}


#endif
