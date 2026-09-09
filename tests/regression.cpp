/***************************************************************************
 * Offscreen regression test for the azimuthal ("globe") projection plugin.
 *
 *   regression <path-to-libmapazimuthal.so>
 *
 * Run with QT_QPA_PLATFORM=offscreen. Exit 0 = all checks passed.
 ***************************************************************************/

#include <seiscomp/gui/map/projection.h>
#include <seiscomp/core/plugin.h>

#include <dlfcn.h>
#include <cmath>
#include <cstdio>
#include <vector>

#include <QImage>
#include <QPainterPath>
#include <QPointF>

using namespace Seiscomp;
using Geo::GeoCoordinate;

static int g_fail = 0;

static void check(bool ok, const char *what) {
	std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
	if ( !ok ) ++g_fail;
}

static void checkClose(double got, double want, double tol, const char *what) {
	const bool ok = std::fabs(got - want) <= tol;
	std::printf("  [%s] %s  (got %.4f, want %.4f +/- %.4f)\n",
	            ok ? "PASS" : "FAIL", what, got, want, tol);
	if ( !ok ) ++g_fail;
}

static int subpaths(const QPainterPath &p) {
	int n = 0;
	for ( int i = 0; i < p.elementCount(); ++i )
		if ( p.elementAt(i).type == QPainterPath::MoveToElement ) ++n;
	return n;
}

// exercise one projection: centre (0,0), 800x600 viewport, no tiles
static void exercise(const char *name, double limbFrac /* |screen| of (90E,0) */) {
	std::printf("\n== %s ==\n", name);
	auto *p = Gui::Map::ProjectionFactory::Create(name);
	if ( !p ) { check(false, "factory Create()"); return; }
	check(!p->isRectangular(), "isRectangular() == false");

	const int W = 800, H = 600, hw = W / 2, hh = H / 2;
	QImage buf(W, H, QImage::Format_ARGB32);
	p->setView(QPointF(0.0, 0.0), 1.0);
	p->draw(buf, true, nullptr);

	const double R = std::min(W, H) * 0.5;   // disc radius in px at zoom 1

	QPoint s;
	// centre of the globe -> centre of the viewport
	check(p->project(s, QPointF(0.0, 0.0)) &&
	      std::abs(s.x() - hw) <= 1 && std::abs(s.y() - hh) <= 1,
	      "(0,0) projects to the viewport centre");

	// north pole straight up from the centre
	check(p->project(s, QPointF(0.0, 90.0)) && std::abs(s.x() - hw) <= 1 && s.y() < hh,
	      "north pole projects straight above the centre");

	// the far side of the globe is not visible
	check(!p->project(s, QPointF(180.0, 0.0)) ||
	      /* az-eq: antipode lands on the rim */ std::hypot(s.x() - hw, s.y() - hh) >= R - 2,
	      "antipode is on the rim or hidden");

	// (90E, 0) sits at a known fraction of the disc radius, on the +x side
	if ( p->project(s, QPointF(90.0, 0.0)) ) {
		checkClose(std::hypot(s.x() - hw, s.y() - hh) / R, limbFrac, 0.02,
		           "(90E,0) radius fraction");
		check(s.x() > hw && std::abs(s.y() - hh) <= 1, "(90E,0) is due east");
	}
	else {
		check(false, "(90E,0) should be visible");
	}

	// round trip for a spread of visible points comfortably inside the disc
	// (near the limb an orthographic inverse is singular, so a round trip
	// through integer screen pixels is precision-limited there by design).
	const struct { double lon, lat; } pts[] = {
		{0, 0}, {30, 20}, {-45, -15}, {50, 35}, {10, 55}, {-40, 5}
	};
	double maxErr = 0.0;
	bool   allOk  = true;
	for ( const auto &q : pts ) {
		QPoint a;
		if ( !p->project(a, QPointF(q.lon, q.lat)) ) { allOk = false; continue; }
		QPointF g;
		if ( !p->unproject(g, a) ) { allOk = false; continue; }
		double dlon = std::fabs(g.x() - q.lon);
		if ( dlon > 180.0 ) dlon = 360.0 - dlon;
		const double e = dlon + std::fabs(g.y() - q.lat);
		if ( e > maxErr ) maxErr = e;
	}
	check(allOk && maxErr < 1.5, "project/unproject round trip < 1.5 deg");

	// a pixel just outside the disc is rejected
	QPointF g;
	check(!p->unproject(g, QPoint(hw + int(R) + 20, hh)),
	      "pixel outside the disc is rejected");

	// bounding box is sane
	const auto &bb = p->boundingBox();
	check(bb.north <= 90.01 && bb.south >= -90.01 &&
	      bb.north > bb.south,
	      "bounding box within range");

	// polygon fully on the near side -> one subpath
	{
		std::vector<GeoCoordinate> box =
			{{10, -10}, {10, 20}, {30, 20}, {30, -10}, {10, -10}};
		QPainterPath path;
		const bool ok = p->project(path, box.size(), box.data(), true, 0,
		                           Gui::Map::NoClip);
		check(ok && subpaths(path) == 1 && path.elementCount() > 5,
		      "near-side polygon -> single interpolated subpath");
	}

	// polygon straddling the limb -> clipped (starts/ends off the disc)
	{
		std::vector<GeoCoordinate> big;
		for ( int lon = -170; lon <= 170; lon += 20 ) big.push_back({0.0, double(lon)});
		QPainterPath path;
		const bool ok = p->project(path, big.size(), big.data(), false, 0,
		                           Gui::Map::NoClip);
		check(ok && subpaths(path) >= 1,
		      "limb-crossing line is clipped into arc(s)");
	}
}

int main(int argc, char **argv) {
	const char *soPath = argc > 1 ? argv[1] : "./libmapazimuthal.so";
	void *h = dlopen(soPath, RTLD_NOW | RTLD_GLOBAL);
	if ( !h ) { std::printf("dlopen(%s): %s\n", soPath, dlerror()); return 2; }

	auto create = reinterpret_cast<Core::Plugin *(*)()>(dlsym(h, "createSCPlugin"));
	check(create != nullptr, "plugin exports createSCPlugin()");
	if ( !create ) return 2;
	create();

	// Orthographic: (90E,0) is at c=90deg -> rho = sin90 = limb -> fraction 1.0
	exercise("Orthographic", 1.0);
	// Stereographic (capped at 90deg): (90E,0) rho = 2 tan45 = 2 = limb -> 1.0
	exercise("Stereographic", 1.0);
	// AzimuthalEquidistant: (90E,0) c=pi/2, rho=pi/2, limb=pi -> fraction 0.5
	exercise("AzimuthalEquidistant", 0.5);

	std::printf("\n%s\n", g_fail ? "*** REGRESSION FAILED ***" : "all checks passed");
	return g_fail ? 1 : 0;
}
