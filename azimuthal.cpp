/***************************************************************************
 * Azimuthal ("globe") map projections for the SeisComP GUI.
 *
 * Spherical transforms after J.P. Snyder, "Map Projections - A Working
 * Manual", USGS Professional Paper 1395 (1987): general azimuthal forward
 * / inverse (pp. 148-150, 157-158, 195-197).
 *
 * Copyright (C) 2025 Mustafa Comoglu
 *
 * This program is free software under the GNU Affero General Public License
 * version 3.0; see the LICENSE file.
 ***************************************************************************/


#include "azimuthal.h"

#include <seiscomp/gui/map/texturecache.ipp>
#include <seiscomp/geo/coordinate.h>

#include <QPainter>

#include <algorithm>
#include <cmath>
#include <math.h>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


namespace Seiscomp {
namespace Gui {
namespace Map {


namespace {

inline double azD2R(double d) { return d * (M_PI / 180.0); }
inline double azR2D(double r) { return r * (180.0 / M_PI); }

inline double clampUnit(double v) {
	return v > 1.0 ? 1.0 : v < -1.0 ? -1.0 : v;
}

//! Normalize a longitude in degrees to (-180, 180].
inline double normLonDeg(double lon) {
	lon = std::fmod(lon + 180.0, 360.0);
	if ( lon < 0.0 ) lon += 360.0;
	return lon - 180.0;
}

//! Interpolation steps for a geographic segment (great circles curve, so
//! straight segments have to be subdivided). Shared by lineSteps() and the
//! poly-line projection.
inline int arcSteps(const QPointF &p0, const QPointF &p1) {
	double dLon = std::fabs(p1.x() - p0.x());
	if ( dLon > 180.0 ) dLon = 360.0 - dLon;
	int steps = int((dLon + std::fabs(p1.y() - p0.y())) / 2.0);
	if ( steps < 2 )   steps = 2;
	if ( steps > 128 ) steps = 128;
	return steps;
}

const double EPS = 1.0e-9;

} // anonymous namespace




// ======================================================================
//  AzimuthalProjection
// ======================================================================
AzimuthalProjection::AzimuthalProjection()
: Projection()
, _lam0(0.0)
, _phi1(0.0)
, _sinPhi1(0.0)
, _cosPhi1(1.0)
, _limb(1.0)
, _ooLimb(1.0) {}
// ----------------------------------------------------------------------




// ----------------------------------------------------------------------
bool AzimuthalProjection::isRectangular() const {
	return false;
}
// ----------------------------------------------------------------------

bool AzimuthalProjection::wantsGridAntialiasing() const {
	return true;
}
// ----------------------------------------------------------------------




// ----------------------------------------------------------------------
void AzimuthalProjection::updateCenter() {
	_lam0    = _visibleCenter.x() * M_PI;          // (lon/180) -> rad
	_phi1    = _visibleCenter.y() * (M_PI / 2.0);  // (lat/90)  -> rad
	_sinPhi1 = std::sin(_phi1);
	_cosPhi1 = std::cos(_phi1);

	_limb   = limbRadius();
	_ooLimb = _limb > 1.0e-12 ? 1.0 / _limb : 1.0;
}
// ----------------------------------------------------------------------




// ----------------------------------------------------------------------
void AzimuthalProjection::centerOn(const QPointF &geoCoords) {
	double lon = normLonDeg(geoCoords.x());
	double lat = geoCoords.y();
	if ( lat >  90.0 ) lat =  90.0;   // a pole is a valid centre
	else if ( lat < -90.0 ) lat = -90.0;

	_center        = QPointF(lon / 180.0, lat / 90.0);
	_visibleCenter = _center;
	updateCenter();
}
// ----------------------------------------------------------------------




// ----------------------------------------------------------------------
//  Forward transform to normalized disc coordinates.
//
//    cos c = sin(phi1) sin(phi) + cos(phi1) cos(phi) cos(lon - lam0)
//    k'    = radius(c) / sin c
//    x     = k' cos(phi) sin(lon - lam0)
//    y     = k' ( cos(phi1) sin(phi) - sin(phi1) cos(phi) cos(lon - lam0) )
//
//  (x, y) are then divided by the limb radius so the visible disc has
//  radius <= 1. Returns false for points beyond the drawn cap.
// ----------------------------------------------------------------------
bool AzimuthalProjection::forwardNorm(double lonRad, double latRad,
                                      double &nx, double &ny) const {
	const double dl     = lonRad - _lam0;
	const double sinPhi = std::sin(latRad);
	const double cosPhi = std::cos(latRad);
	const double sinDl  = std::sin(dl);
	const double cosDl  = std::cos(dl);

	const double cosC = clampUnit(_sinPhi1 * sinPhi + _cosPhi1 * cosPhi * cosDl);
	if ( cosC < std::cos(cMax()) - EPS )
		return false;                       // far side of the globe

	const double c    = std::acos(cosC);
	const double sinC = std::sin(c);

	double kp;
	if ( c < 1.0e-9 )
		kp = 1.0;                           // all azimuthals -> 1 at the centre
	else
		kp = radius(c, sinC, cosC) / sinC;

	nx = kp * cosPhi * sinDl * _ooLimb;
	ny = kp * (_cosPhi1 * sinPhi - _sinPhi1 * cosPhi * cosDl) * _ooLimb;
	return true;
}
// ----------------------------------------------------------------------




// ----------------------------------------------------------------------
//  Inverse of forwardNorm(): normalized disc coordinates -> lon/lat [rad].
//  @p n2 == nx*nx + ny*ny. sinCos() gives sin/cos of the angular distance
//  in closed form, so the only transcendentals left are the asin / atan2
//  that actually recover the geographic coordinate.
// ----------------------------------------------------------------------
bool AzimuthalProjection::inverseNorm(double nx, double ny, double n2,
                                      double &lonRad, double &latRad) const {
	if ( n2 < 1.0e-24 ) {
		lonRad = _lam0;
		latRad = _phi1;
		return true;
	}

	const double rho2 = n2 * _limb * _limb;
	const double rho  = std::sqrt(rho2);
	if ( rho > _limb + EPS )
		return false;                         // beyond the drawn limb

	double sinC, cosC;
	sinCos(rho, rho2, sinC, cosC);

	const double xp = nx * _limb;
	const double yp = ny * _limb;

	latRad = std::asin(clampUnit(cosC * _sinPhi1 + yp * sinC * _cosPhi1 / rho));
	lonRad = _lam0 + std::atan2(xp * sinC,
	                            rho * _cosPhi1 * cosC - yp * _sinPhi1 * sinC);
	return true;
}
// ----------------------------------------------------------------------




// ----------------------------------------------------------------------
bool AzimuthalProjection::project(QPoint &screenCoords,
                                  const QPointF &geoCoords) const {
	if ( _scale <= 0.0 )
		return false;

	double lat = geoCoords.y();
	if ( lat >  90.0 ) lat =  90.0;
	else if ( lat < -90.0 ) lat = -90.0;

	double nx, ny;
	if ( !forwardNorm(azD2R(geoCoords.x()), azD2R(lat), nx, ny) )
		return false;

	screenCoords.setX(int(std::lround(_halfWidth  + nx * _scale)));
	screenCoords.setY(int(std::lround(_halfHeight - ny * _scale)));
	return true;
}
// ----------------------------------------------------------------------




// ----------------------------------------------------------------------
bool AzimuthalProjection::unproject(QPointF &geoCoords,
                                    const QPoint &screenCoords) const {
	if ( _scale <= 0.0 )
		return false;

	const double nx = (double(screenCoords.x()) - _halfWidth) / _scale;
	const double ny = (double(_halfHeight) - screenCoords.y()) / _scale;
	const double n2 = nx * nx + ny * ny;

	if ( n2 > 1.0 + 1.0e-6 )
		return false;                       // outside the disc

	double lonRad, latRad;
	if ( !inverseNorm(nx, ny, n2, lonRad, latRad) )
		return false;

	double lat = azR2D(latRad);
	if ( lat >  90.0 ) lat =  90.0;
	else if ( lat < -90.0 ) lat = -90.0;

	geoCoords.setX(normLonDeg(azR2D(lonRad)));
	geoCoords.setY(lat);
	return true;
}
// ----------------------------------------------------------------------




// ----------------------------------------------------------------------
int AzimuthalProjection::lineSteps(const QPointF &p0, const QPointF &p1) {
	// Great circles project to curves (only lines through the centre stay
	// straight), so geographic segments must be interpolated.
	return arcSteps(p0, p1);
}
// ----------------------------------------------------------------------




// ----------------------------------------------------------------------
bool AzimuthalProjection::lineTo(QPainter &painter, const QPointF &to) {
	const QPoint from      = _cursor;
	const bool   fromValid = _cursorVisible;

	QPoint p;
	const bool visible = project(p, to);

	bool drawn = false;
	if ( fromValid && visible ) {
		// screen-jump guard: an azimuthal edge that wraps past the antipode
		// would otherwise draw a chord straight across the disc
		const double jump = std::hypot(double(p.x() - from.x()),
		                               double(p.y() - from.y()));
		const bool onScreen = (from.y() >= 0 || p.y() >= 0)
		                   && (from.y() < _height || p.y() < _height)
		                   && (from.x() >= 0 || p.x() >= 0)
		                   && (from.x() < _width  || p.x() < _width);
		if ( onScreen && jump <= _scale ) {
			painter.drawLine(from, p);
			drawn = true;
		}
	}

	_cursor        = p;
	_cursorVisible = visible;
	return drawn;
}
// ----------------------------------------------------------------------




// ----------------------------------------------------------------------
//  Map rasterization: for every pixel inside the disc, inverse-project and
//  sample the tile cache; pixels outside the disc / on the far side stay
//  fully transparent (the canvas allocates an ARGB32 buffer).
// ----------------------------------------------------------------------
void AzimuthalProjection::render(QImage &img, bool highQuality,
                                 TextureCache *cache) {
	_screenRadius = std::min(_width, _height) * 0.5;
	if ( _screenRadius <= 0.0 ) {
		img.fill(qRgba(0, 0, 0, 0));
		return;
	}

	qreal radius = _screenRadius * _radius;        // _radius == zoom
	if ( radius < _screenRadius )                  // never smaller than the viewport
		radius = _screenRadius;

	setVisibleRadius(radius / _screenRadius);      // -> _scale == radius
	updateCenter();

	const int  w = img.width();
	const int  h = img.height();
	const QRgb transparent = qRgba(0, 0, 0, 0);

	if ( cache == nullptr ) {
		img.fill(transparent);
		return;
	}

	// Texture pyramid level (same heuristic as the built-in projections).
	qreal pixelRatio = 2.0 * _scale / cache->tileHeight();
	const bool mercatorTiles = cache->isMercatorProjected();
	if ( mercatorTiles )
		pixelRatio *= 2;
	if ( pixelRatio < 1.0 ) pixelRatio = 1.0;
	int level = int(std::log(pixelRatio) / std::log(2.0) + 0.7);
	if ( level < 0 ) level = 0;
	if ( level > cache->maxLevel() ) level = cache->maxLevel();

	const double MERC_LAT_LIMIT = azD2R(85.05113);
	const double ooScale = 1.0 / _scale;
	const double fh      = double(Coord::fraction_half_max);

	// The disc is centred in the viewport; its bounding box limits the rows.
	int y0 = int(_halfHeight - _scale) - 1; if ( y0 < 0 ) y0 = 0;
	int y1 = int(_halfHeight + _scale) + 1; if ( y1 > h ) y1 = h;

	for ( int iy = 0; iy < h; ++iy ) {
		QRgb *scan = reinterpret_cast<QRgb*>(img.scanLine(iy));

		if ( iy < y0 || iy >= y1 ) {
			for ( int ix = 0; ix < w; ++ix ) scan[ix] = transparent;
			continue;
		}

		const double ny  = (double(_halfHeight) - iy) * ooScale;
		const double ny2 = ny * ny;

		// Columns inside the disc for this row: nx*nx <= 1 - ny2.
		int xl = w, xr = -1;
		if ( ny2 < 1.0 ) {
			const double halfW = std::sqrt(1.0 - ny2) * _scale;
			xl = int(std::ceil (_halfWidth - halfW));
			xr = int(std::floor(_halfWidth + halfW));
			if ( xl < 0 ) xl = 0;
			if ( xr > w - 1 ) xr = w - 1;
		}

		int ix = 0;
		for ( ; ix < xl; ++ix ) scan[ix] = transparent;

		for ( ; ix <= xr; ++ix ) {
			const double nx = (double(ix) - _halfWidth) * ooScale;
			const double n2 = nx * nx + ny2;

			double lonRad, latRad;
			if ( !inverseNorm(nx, ny, n2, lonRad, latRad) ) {
				scan[ix] = transparent;
				continue;
			}

			// No wrap needed: getTexel() masks U to its fractional part.
			Coord u;
			u.value = Coord::value_type((lonRad * (1.0 / M_PI) + 1.0) * fh);

			Coord v;
			if ( mercatorTiles ) {
				double p = latRad;
				if ( p >  MERC_LAT_LIMIT ) p =  MERC_LAT_LIMIT;
				else if ( p < -MERC_LAT_LIMIT ) p = -MERC_LAT_LIMIT;
				const double my = std::asinh(std::tan(p)) / M_PI;
				v.value = Coord::value_type((1.0 - my) * fh);
			}
			else {
				v.value = Coord::value_type((1.0 - latRad * (2.0 / M_PI)) * fh);
			}

			QRgb c;
			if ( highQuality )
				cache->getTexelBilinear(c, u, v, level);
			else
				cache->getTexel(c, u, v, level);

			scan[ix] = c | 0xff000000u;
		}

		for ( ; ix < w; ++ix ) scan[ix] = transparent;
	}
}
// ----------------------------------------------------------------------




// ----------------------------------------------------------------------
//  Visible geographic extent. If a pole projects inside the viewport the
//  longitude range is the whole world; otherwise a coarse screen grid is
//  back-projected and the extent accumulated relative to the centre.
// ----------------------------------------------------------------------
void AzimuthalProjection::updateBoundingBox() {
	_mapBoundingBox.reset();
	if ( _width <= 0 || _height <= 0 || _scale <= 0.0 )
		return;

	const double centerLon = _visibleCenter.x() * 180.0;
	const int    step      = std::max(1, std::min(_width, _height) / 64);

	bool   have = false;
	double west = 0.0, east = 0.0, north = -90.0, south = 90.0;

	QPointF g;
	for ( int y = 0; y < _height; y += step ) {
		for ( int x = 0; x < _width; x += step ) {
			if ( !unproject(g, QPoint(x, y)) )
				continue;

			const double dLon = Geo::GeoCoordinate::distanceLon(g.x(), centerLon);
			if ( !have ) { west = east = dLon; have = true; }
			else {
				if ( dLon < west ) west = dLon;
				if ( dLon > east ) east = dLon;
			}
			if ( g.y() > north ) north = g.y();
			if ( g.y() < south ) south = g.y();
		}
	}

	if ( !have ) {
		_mapBoundingBox.west  = -180.0; _mapBoundingBox.east  = 180.0;
		_mapBoundingBox.south =  -90.0; _mapBoundingBox.north =  90.0;
		return;
	}

	QPoint p;
	const bool nPole = project(p, QPointF(0.0,  90.0)) &&
	                   p.x() >= 0 && p.x() < _width && p.y() >= 0 && p.y() < _height;
	const bool sPole = project(p, QPointF(0.0, -90.0)) &&
	                   p.x() >= 0 && p.x() < _width && p.y() >= 0 && p.y() < _height;

	if ( nPole ) north = 90.0;
	if ( sPole ) south = -90.0;

	if ( nPole || sPole || (east - west) >= 359.0 ) {
		_mapBoundingBox.west = -180.0;
		_mapBoundingBox.east =  180.0;
	}
	else {
		_mapBoundingBox.west = Geo::GeoCoordinate::normalizeLon(west + centerLon);
		_mapBoundingBox.east = Geo::GeoCoordinate::normalizeLon(east + centerLon);
	}

	_mapBoundingBox.north = std::min( 90.0, north);
	_mapBoundingBox.south = std::max(-90.0, south);
}
// ----------------------------------------------------------------------




// ----------------------------------------------------------------------
//  Project a geographic poly-line. Edges are interpolated (great circles
//  curve) and the path is broken wherever it leaves the visible disc, so a
//  feature straddling the limb is clipped into arcs rather than wrapping
//  across the globe.
// ----------------------------------------------------------------------
bool AzimuthalProjection::project(QPainterPath &screenPath, size_t n,
                                  const Geo::GeoCoordinate *poly, bool closed,
                                  uint minPixelDist, ClipHint) const {
	if ( n < 2 || !poly || _scale <= 0.0 )
		return false;

	const double minDeg = (minPixelDist > 0)
	                      ? double(minPixelDist) / pixelPerDegree()
	                      : 0.0;

	// Decimate to the requested roughness first.
	std::vector<QPointF> v;
	v.reserve(n);
	v.push_back(QPointF(poly[0].lon, poly[0].lat));
	for ( size_t i = 1; i < n; ++i ) {
		double dLon = poly[i].lon - v.back().x();
		if ( dLon >  180.0 ) dLon -= 360.0;
		else if ( dLon < -180.0 ) dLon += 360.0;
		const bool keepLast = !closed && (i == n - 1);
		if ( !keepLast && minDeg > 0.0
		  && std::fabs(dLon) <= minDeg
		  && std::fabs(poly[i].lat - v.back().y()) <= minDeg )
			continue;
		v.push_back(QPointF(v.back().x() + dLon, poly[i].lat));
	}
	if ( closed && v.size() > 2 )
		v.push_back(v.front());
	if ( v.size() < 2 )
		return false;

	QPoint p, prev;
	bool penDown = false;
	bool any = false;

	// A real adjacent interpolation step never jumps more than a fraction
	// of the disc; a larger jump means the arc wrapped past the antipode
	// (azimuth flips ~180 deg there), so lift the pen instead of drawing a
	// chord across the globe.
	const double maxJump = _scale;   // one disc radius

	// NOTE: 'emit' is a Qt macro - do not name a local that.
	auto plot = [&](double lon, double lat) {
		if ( project(p, QPointF(lon, lat)) ) {
			if ( penDown && std::hypot(double(p.x() - prev.x()),
			                           double(p.y() - prev.y())) <= maxJump )
				screenPath.lineTo(p);
			else
				screenPath.moveTo(p);
			penDown = true;
			prev = p;
			any = true;
		}
		else {
			penDown = false;                  // pen up: the arc leaves the disc
		}
	};

	plot(v[0].x(), v[0].y());
	for ( size_t i = 1; i < v.size(); ++i ) {
		const int steps = arcSteps(v[i - 1], v[i]);
		for ( int s = 1; s <= steps; ++s ) {
			const double t = double(s) / steps;
			plot(v[i - 1].x() + t * (v[i].x() - v[i - 1].x()),
			     v[i - 1].y() + t * (v[i].y() - v[i - 1].y()));
		}
	}

	return any && !screenPath.isEmpty();
}
// ----------------------------------------------------------------------




}
}
}
