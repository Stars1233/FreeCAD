/***************************************************************************
 *   Copyright (c) 2014 Victor Titov (DeepSOIC) <vv.titov@gmail.com>       *
 *                                                                         *
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

#define DEBUG_DERIVS 0
#if DEBUG_DERIVS
#endif

#include <cassert>

#include "Geo.h"


namespace GCS
{

//----------------Point
int Point::PushOwnParams(VEC_pD& pvec) const
{
    int cnt = 0;
    pvec.push_back(x);
    cnt++;
    pvec.push_back(y);
    cnt++;
    return cnt;
}

void Point::ReconstructOnNewPvec(VEC_pD& pvec, int& cnt)
{
    x = pvec[cnt];
    cnt++;
    y = pvec[cnt];
    cnt++;
}

//----------------DeriVector2
DeriVector2::DeriVector2(const Point& p, const double* derivparam)
    : x(*p.x)
    , dx(0.0)
    , y(*p.y)
    , dy(0.0)
{
    if (derivparam == p.x) {
        dx = 1.0;
    }
    if (derivparam == p.y) {
        dy = 1.0;
    }
}

double DeriVector2::length(double& dlength) const
{
    double l = length();
    if (l == 0) {
        dlength = 1.0;
        return l;
    }
    dlength = (x * dx + y * dy) / l;
    return l;
}

DeriVector2 DeriVector2::getNormalized() const
{
    double l = length();
    if (l == 0.0) {
        return DeriVector2(0, 0, dx, dy);
    }
    DeriVector2 rtn;
    rtn.x = x / l;
    rtn.y = y / l;
    // first, simply scale the derivative accordingly.
    rtn.dx = dx / l;
    rtn.dy = dy / l;
    // next, remove the collinear part of dx,dy (make a projection onto a normal)
    double dsc = rtn.dx * rtn.x + rtn.dy * rtn.y;  // scalar product d*v
    rtn.dx -= dsc * rtn.x;                         // subtract the projection
    rtn.dy -= dsc * rtn.y;
    return rtn;
}

double DeriVector2::scalarProd(const DeriVector2& v2, double* dprd) const
{
    if (dprd) {
        *dprd = dx * v2.x + x * v2.dx + dy * v2.y + y * v2.dy;
    }
    return x * v2.x + y * v2.y;
}

DeriVector2 DeriVector2::divD(double val, double dval) const
{
    return {x / val, y / val, dx / val - x * dval / (val * val), dy / val - y * dval / (val * val)};
}

double DeriVector2::crossProdNorm(const DeriVector2& v2, double& dprd) const
{
    dprd = dx * v2.y + x * v2.dy - dy * v2.x - y * v2.dx;
    return x * v2.y - y * v2.x;
}

DeriVector2 Curve::Value(double /*u*/, double /*du*/, const double* /*derivparam*/) const
{
    assert(false /*Value() is not implemented*/);
    return {};
}

//----------------Line

DeriVector2 Line::CalculateNormal(const Point& p, const double* derivparam) const
{
    (void)p;
    DeriVector2 p1v(p1, derivparam);
    DeriVector2 p2v(p2, derivparam);

    return p2v.subtr(p1v).rotate90ccw();
}

DeriVector2 Line::Value(double u, double du, const double* derivparam) const
{
    DeriVector2 p1v(p1, derivparam);
    DeriVector2 p2v(p2, derivparam);

    DeriVector2 line_vec = p2v.subtr(p1v);
    return p1v.sum(line_vec.multD(u, du));
}

int Line::PushOwnParams(VEC_pD& pvec)
{
    int cnt = 0;
    pvec.push_back(p1.x);
    cnt++;
    pvec.push_back(p1.y);
    cnt++;
    pvec.push_back(p2.x);
    cnt++;
    pvec.push_back(p2.y);
    cnt++;
    return cnt;
}
void Line::ReconstructOnNewPvec(VEC_pD& pvec, int& cnt)
{
    p1.x = pvec[cnt];
    cnt++;
    p1.y = pvec[cnt];
    cnt++;
    p2.x = pvec[cnt];
    cnt++;
    p2.y = pvec[cnt];
    cnt++;
}
Line* Line::Copy()
{
    return new Line(*this);
}


//---------------circle

DeriVector2 Circle::CalculateNormal(const Point& p, const double* derivparam) const
{
    DeriVector2 cv(center, derivparam);
    DeriVector2 pv(p, derivparam);

    return cv.subtr(pv);
}

DeriVector2 Circle::Value(double u, double du, const double* derivparam) const
{
    DeriVector2 cv(center, derivparam);
    double r, dr;
    r = *(this->rad);
    dr = (derivparam == this->rad) ? 1.0 : 0.0;
    DeriVector2 ex(r, 0.0, dr, 0.0);
    DeriVector2 ey = ex.rotate90ccw();
    double si, dsi, co, dco;
    si = std::sin(u);
    dsi = du * std::cos(u);
    co = std::cos(u);
    dco = du * (-std::sin(u));
    return cv.sum(ex.multD(co, dco).sum(ey.multD(si, dsi)));
}

int Circle::PushOwnParams(VEC_pD& pvec)
{
    int cnt = 0;
    pvec.push_back(center.x);
    cnt++;
    pvec.push_back(center.y);
    cnt++;
    pvec.push_back(rad);
    cnt++;
    return cnt;
}
void Circle::ReconstructOnNewPvec(VEC_pD& pvec, int& cnt)
{
    center.x = pvec[cnt];
    cnt++;
    center.y = pvec[cnt];
    cnt++;
    rad = pvec[cnt];
    cnt++;
}
Circle* Circle::Copy()
{
    return new Circle(*this);
}

//------------arc
int Arc::PushOwnParams(VEC_pD& pvec)
{
    int cnt = 0;
    cnt += Circle::PushOwnParams(pvec);
    pvec.push_back(start.x);
    cnt++;
    pvec.push_back(start.y);
    cnt++;
    pvec.push_back(end.x);
    cnt++;
    pvec.push_back(end.y);
    cnt++;
    pvec.push_back(startAngle);
    cnt++;
    pvec.push_back(endAngle);
    cnt++;
    return cnt;
}
void Arc::ReconstructOnNewPvec(VEC_pD& pvec, int& cnt)
{
    Circle::ReconstructOnNewPvec(pvec, cnt);
    start.x = pvec[cnt];
    cnt++;
    start.y = pvec[cnt];
    cnt++;
    end.x = pvec[cnt];
    cnt++;
    end.y = pvec[cnt];
    cnt++;
    startAngle = pvec[cnt];
    cnt++;
    endAngle = pvec[cnt];
    cnt++;
}
Arc* Arc::Copy()
{
    return new Arc(*this);
}


//--------------ellipse

// this function is exposed to allow reusing pre-filled derivectors in constraints code
double Ellipse::getRadMaj(const DeriVector2& center,
                          const DeriVector2& f1,
                          double b,
                          double db,
                          double& ret_dRadMaj) const
{
    double cf, dcf;
    cf = f1.subtr(center).length(dcf);
    DeriVector2 hack(
        b,
        cf,
        db,
        dcf);  // hack = a nonsense vector to calculate major radius with derivatives, useful just
               // because the calculation formula is the same as vector length formula
    return hack.length(ret_dRadMaj);
}

// returns major radius. The derivative by derivparam is returned into ret_dRadMaj argument.
double Ellipse::getRadMaj(double* derivparam, double& ret_dRadMaj) const
{
    DeriVector2 c(center, derivparam);
    DeriVector2 f1(focus1, derivparam);
    return getRadMaj(c, f1, *radmin, radmin == derivparam ? 1.0 : 0.0, ret_dRadMaj);
}

// returns the major radius (plain value, no derivatives)
double Ellipse::getRadMaj() const
{
    double dradmaj;  // dummy
    return getRadMaj(nullptr, dradmaj);
}

DeriVector2 Ellipse::CalculateNormal(const Point& p, const double* derivparam) const
{
    // fill some vectors in
    DeriVector2 cv(center, derivparam);
    DeriVector2 f1v(focus1, derivparam);
    DeriVector2 pv(p, derivparam);

    // calculation.
    // focus2:
    DeriVector2 f2v = cv.linCombi(2.0, f1v, -1.0);  // 2*cv - f1v

    // pf1, pf2 = vectors from p to focus1,focus2
    DeriVector2 pf1 = f1v.subtr(pv);
    DeriVector2 pf2 = f2v.subtr(pv);

    // return sum of normalized pf2, pf2
    return pf1.getNormalized().sum(pf2.getNormalized());
}

DeriVector2 Ellipse::Value(double u, double du, const double* derivparam) const
{
    // In local coordinate system, value() of ellipse is:
    //(a*cos(u), b*sin(u))
    // In global, it is (vector formula):
    // center + a_vec*cos(u) + b_vec*sin(u).
    // That's what is being computed here.

    // <construct a_vec, b_vec>
    DeriVector2 c(this->center, derivparam);
    DeriVector2 f1(this->focus1, derivparam);

    DeriVector2 emaj = f1.subtr(c).getNormalized();
    DeriVector2 emin = emaj.rotate90ccw();
    double b, db;
    b = *(this->radmin);
    db = this->radmin == derivparam ? 1.0 : 0.0;
    double a, da;
    a = this->getRadMaj(c, f1, b, db, da);
    DeriVector2 a_vec = emaj.multD(a, da);
    DeriVector2 b_vec = emin.multD(b, db);
    // </construct a_vec, b_vec>

    // sin, cos with derivatives:
    double co, dco, si, dsi;
    co = std::cos(u);
    dco = -std::sin(u) * du;
    si = std::sin(u);
    dsi = std::cos(u) * du;

    // point of ellipse at parameter value of u, in global coordinates
    return a_vec.multD(co, dco).sum(b_vec.multD(si, dsi)).sum(c);
}

int Ellipse::PushOwnParams(VEC_pD& pvec)
{
    int cnt = 0;
    pvec.push_back(center.x);
    cnt++;
    pvec.push_back(center.y);
    cnt++;
    pvec.push_back(focus1.x);
    cnt++;
    pvec.push_back(focus1.y);
    cnt++;
    pvec.push_back(radmin);
    cnt++;
    return cnt;
}
void Ellipse::ReconstructOnNewPvec(VEC_pD& pvec, int& cnt)
{
    center.x = pvec[cnt];
    cnt++;
    center.y = pvec[cnt];
    cnt++;
    focus1.x = pvec[cnt];
    cnt++;
    focus1.y = pvec[cnt];
    cnt++;
    radmin = pvec[cnt];
    cnt++;
}
Ellipse* Ellipse::Copy()
{
    return new Ellipse(*this);
}


//---------------arc of ellipse
int ArcOfEllipse::PushOwnParams(VEC_pD& pvec)
{
    int cnt = 0;
    cnt += Ellipse::PushOwnParams(pvec);
    pvec.push_back(start.x);
    cnt++;
    pvec.push_back(start.y);
    cnt++;
    pvec.push_back(end.x);
    cnt++;
    pvec.push_back(end.y);
    cnt++;
    pvec.push_back(startAngle);
    cnt++;
    pvec.push_back(endAngle);
    cnt++;
    return cnt;
}
void ArcOfEllipse::ReconstructOnNewPvec(VEC_pD& pvec, int& cnt)
{
    Ellipse::ReconstructOnNewPvec(pvec, cnt);
    start.x = pvec[cnt];
    cnt++;
    start.y = pvec[cnt];
    cnt++;
    end.x = pvec[cnt];
    cnt++;
    end.y = pvec[cnt];
    cnt++;
    startAngle = pvec[cnt];
    cnt++;
    endAngle = pvec[cnt];
    cnt++;
}
ArcOfEllipse* ArcOfEllipse::Copy()
{
    return new ArcOfEllipse(*this);
}

//---------------hyperbola

// this function is exposed to allow reusing pre-filled derivectors in constraints code
double Hyperbola::getRadMaj(const DeriVector2& center,
                            const DeriVector2& f1,
                            double b,
                            double db,
                            double& ret_dRadMaj) const
{
    double cf, dcf;
    cf = f1.subtr(center).length(dcf);
    double a, da;
    a = sqrt(cf * cf - b * b);
    da = (dcf * cf - db * b) / a;
    ret_dRadMaj = da;
    return a;
}

// returns major radius. The derivative by derivparam is returned into ret_dRadMaj argument.
double Hyperbola::getRadMaj(double* derivparam, double& ret_dRadMaj) const
{
    DeriVector2 c(center, derivparam);
    DeriVector2 f1(focus1, derivparam);
    return getRadMaj(c, f1, *radmin, radmin == derivparam ? 1.0 : 0.0, ret_dRadMaj);
}

// returns the major radius (plain value, no derivatives)
double Hyperbola::getRadMaj() const
{
    double dradmaj;  // dummy
    return getRadMaj(nullptr, dradmaj);
}

DeriVector2 Hyperbola::CalculateNormal(const Point& p, const double* derivparam) const
{
    // fill some vectors in
    DeriVector2 cv(center, derivparam);
    DeriVector2 f1v(focus1, derivparam);
    DeriVector2 pv(p, derivparam);

    // calculation.
    // focus2:
    DeriVector2 f2v = cv.linCombi(2.0, f1v, -1.0);  // 2*cv - f1v

    // pf1, pf2 = vectors from p to focus1,focus2
    DeriVector2 pf1 = f1v.subtr(pv).mult(
        -1.0);  // <--- differs from ellipse normal calculation code by inverting this vector
    DeriVector2 pf2 = f2v.subtr(pv);

    // return sum of normalized pf2, pf2
    return pf1.getNormalized().sum(pf2.getNormalized());
}

DeriVector2 Hyperbola::Value(double u, double du, const double* derivparam) const
{

    // In local coordinate system, value() of hyperbola is:
    //(a*cosh(u), b*sinh(u))
    // In global, it is (vector formula):
    // center + a_vec*cosh(u) + b_vec*sinh(u).
    // That's what is being computed here.

    // <construct a_vec, b_vec>
    DeriVector2 c(this->center, derivparam);
    DeriVector2 f1(this->focus1, derivparam);

    DeriVector2 emaj = f1.subtr(c).getNormalized();
    DeriVector2 emin = emaj.rotate90ccw();
    double b, db;
    b = *(this->radmin);
    db = this->radmin == derivparam ? 1.0 : 0.0;
    double a, da;
    a = this->getRadMaj(c, f1, b, db, da);
    DeriVector2 a_vec = emaj.multD(a, da);
    DeriVector2 b_vec = emin.multD(b, db);
    // </construct a_vec, b_vec>

    // sinh, cosh with derivatives:
    double co, dco, si, dsi;
    co = std::cosh(u);
    dco = std::sinh(u) * du;
    si = std::sinh(u);
    dsi = std::cosh(u) * du;

    // point of hyperbola at parameter value of u, in global coordinates
    return a_vec.multD(co, dco).sum(b_vec.multD(si, dsi)).sum(c);
}

int Hyperbola::PushOwnParams(VEC_pD& pvec)
{
    int cnt = 0;
    pvec.push_back(center.x);
    cnt++;
    pvec.push_back(center.y);
    cnt++;
    pvec.push_back(focus1.x);
    cnt++;
    pvec.push_back(focus1.y);
    cnt++;
    pvec.push_back(radmin);
    cnt++;
    return cnt;
}
void Hyperbola::ReconstructOnNewPvec(VEC_pD& pvec, int& cnt)
{
    center.x = pvec[cnt];
    cnt++;
    center.y = pvec[cnt];
    cnt++;
    focus1.x = pvec[cnt];
    cnt++;
    focus1.y = pvec[cnt];
    cnt++;
    radmin = pvec[cnt];
    cnt++;
}
Hyperbola* Hyperbola::Copy()
{
    return new Hyperbola(*this);
}

//--------------- arc of hyperbola
int ArcOfHyperbola::PushOwnParams(VEC_pD& pvec)
{
    int cnt = 0;
    cnt += Hyperbola::PushOwnParams(pvec);
    pvec.push_back(start.x);
    cnt++;
    pvec.push_back(start.y);
    cnt++;
    pvec.push_back(end.x);
    cnt++;
    pvec.push_back(end.y);
    cnt++;
    pvec.push_back(startAngle);
    cnt++;
    pvec.push_back(endAngle);
    cnt++;
    return cnt;
}
void ArcOfHyperbola::ReconstructOnNewPvec(VEC_pD& pvec, int& cnt)
{
    Hyperbola::ReconstructOnNewPvec(pvec, cnt);
    start.x = pvec[cnt];
    cnt++;
    start.y = pvec[cnt];
    cnt++;
    end.x = pvec[cnt];
    cnt++;
    end.y = pvec[cnt];
    cnt++;
    startAngle = pvec[cnt];
    cnt++;
    endAngle = pvec[cnt];
    cnt++;
}
ArcOfHyperbola* ArcOfHyperbola::Copy()
{
    return new ArcOfHyperbola(*this);
}

//---------------parabola

DeriVector2 Parabola::CalculateNormal(const Point& p, const double* derivparam) const
{
    // fill some vectors in
    DeriVector2 cv(vertex, derivparam);
    DeriVector2 f1v(focus1, derivparam);
    DeriVector2 pv(p, derivparam);

    // the normal is the vector from the focus to the intersection of ano thru the point p and
    // direction of the symmetry axis of the parabola with the directrix. As both point to directrix
    // and point to focus are of equal magnitude, we can work with unitary vectors to calculate the
    // normal, substraction of those vectors.

    return cv.subtr(f1v).getNormalized().subtr(f1v.subtr(pv).getNormalized());
}

DeriVector2 Parabola::Value(double u, double du, const double* derivparam) const
{

    // In local coordinate system, value() of parabola is:
    // P(U) = O + U*U/(4.*F)*XDir + U*YDir

    DeriVector2 c(this->vertex, derivparam);
    DeriVector2 f1(this->focus1, derivparam);

    DeriVector2 fv = f1.subtr(c);

    double f, df;

    f = fv.length(df);

    DeriVector2 xdir = fv.getNormalized();
    DeriVector2 ydir = xdir.rotate90ccw();

    DeriVector2 dirx = xdir.multD(u, du).multD(u, du).divD(4 * f, 4 * df);
    DeriVector2 diry = ydir.multD(u, du);

    DeriVector2 dir = dirx.sum(diry);

    DeriVector2 ret;  // point of parabola at parameter value of u, in global coordinates

    ret = c.sum(dir);

    return ret;
}

int Parabola::PushOwnParams(VEC_pD& pvec)
{
    int cnt = 0;
    pvec.push_back(vertex.x);
    cnt++;
    pvec.push_back(vertex.y);
    cnt++;
    pvec.push_back(focus1.x);
    cnt++;
    pvec.push_back(focus1.y);
    cnt++;
    return cnt;
}

void Parabola::ReconstructOnNewPvec(VEC_pD& pvec, int& cnt)
{
    vertex.x = pvec[cnt];
    cnt++;
    vertex.y = pvec[cnt];
    cnt++;
    focus1.x = pvec[cnt];
    cnt++;
    focus1.y = pvec[cnt];
    cnt++;
}

Parabola* Parabola::Copy()
{
    return new Parabola(*this);
}

//--------------- arc of hyperbola
int ArcOfParabola::PushOwnParams(VEC_pD& pvec)
{
    int cnt = 0;
    cnt += Parabola::PushOwnParams(pvec);
    pvec.push_back(start.x);
    cnt++;
    pvec.push_back(start.y);
    cnt++;
    pvec.push_back(end.x);
    cnt++;
    pvec.push_back(end.y);
    cnt++;
    pvec.push_back(startAngle);
    cnt++;
    pvec.push_back(endAngle);
    cnt++;
    return cnt;
}
void ArcOfParabola::ReconstructOnNewPvec(VEC_pD& pvec, int& cnt)
{
    Parabola::ReconstructOnNewPvec(pvec, cnt);
    start.x = pvec[cnt];
    cnt++;
    start.y = pvec[cnt];
    cnt++;
    end.x = pvec[cnt];
    cnt++;
    end.y = pvec[cnt];
    cnt++;
    startAngle = pvec[cnt];
    cnt++;
    endAngle = pvec[cnt];
    cnt++;
}
ArcOfParabola* ArcOfParabola::Copy()
{
    return new ArcOfParabola(*this);
}

// bspline
DeriVector2 BSpline::CalculateNormal(const Point& p, const double* derivparam) const
{
    // even if this method is call CalculateNormal, the returned vector is not the normal strictu
    // sensus but a normal vector, where the vector should point to the left when one walks along
    // the curve from start to end.
    //
    // https://forum.freecad.org/viewtopic.php?f=10&t=26312#p209486

    if (mult[0] > degree && mult[mult.size() - 1] > degree) {
        // if endpoints through end poles
        if (*p.x == *start.x && *p.y == *start.y) {
            // and you are asking about the normal at start point
            // then tangency is defined by first to second poles
            DeriVector2 endpt(this->poles[1], derivparam);
            DeriVector2 spt(this->poles[0], derivparam);

            DeriVector2 tg = endpt.subtr(spt);
            return tg.rotate90ccw();
        }
        if (*p.x == *end.x && *p.y == *end.y) {
            // and you are asking about the normal at end point
            // then tangency is defined by last to last but one poles
            DeriVector2 endpt(this->poles[poles.size() - 1], derivparam);
            DeriVector2 spt(this->poles[poles.size() - 2], derivparam);

            DeriVector2 tg = endpt.subtr(spt);
            return tg.rotate90ccw();
        }
        // another point and we have no clue until we implement De Boor
        return {};
    }
    // either periodic or abnormal endpoint multiplicity, we have no clue so currently
    // unsupported
    return {};
}

DeriVector2 BSpline::CalculateNormal(const double* param, const double* derivparam) const
{
    // TODO: is there any advantage in making this a `static`?
    size_t startpole = 0;
    for (size_t j = 1; j < mult.size() && *(knots[j]) <= *param; ++j) {
        startpole += mult[j];
    }
    if (!periodic && startpole >= poles.size()) {
        startpole = poles.size() - degree - 1;
    }

    auto polexat = [&](size_t i) {
        return poles[(startpole + i) % poles.size()].x;
    };
    auto poleyat = [&](size_t i) {
        return poles[(startpole + i) % poles.size()].y;
    };
    auto weightat = [&](size_t i) {
        return weights[(startpole + i) % weights.size()];
    };

    double xsum, xslopesum;
    double ysum, yslopesum;
    double wsum, wslopesum;

    valueHomogenous(*param, &xsum, &ysum, &wsum, &xslopesum, &yslopesum, &wslopesum);

    // Tangent vector
    // This should in principle be identical to error gradient wrt curve parameter in
    // point-on-object
    DeriVector2 result(wsum * xslopesum - wslopesum * xsum, wsum * yslopesum - wslopesum * ysum);

    size_t numpoints = degree + 1;

    // get dx, dy of the normal as well
    for (size_t i = 0; i < numpoints; ++i) {
        if (derivparam != polexat(i) && derivparam != poleyat(i) && derivparam != weightat(i)) {
            continue;
        }

        VEC_D d(numpoints);
        d[i] = 1;
        double factor = splineValue(*param, startpole + degree, degree, d, flattenedknots);
        VEC_D sd(numpoints - 1);
        if (i > 0) {
            sd[i - 1] =
                1.0 / (flattenedknots[startpole + i + degree] - flattenedknots[startpole + i]);
        }
        if (i < numpoints - 1) {
            sd[i] = -1.0
                / (flattenedknots[startpole + i + 1 + degree] - flattenedknots[startpole + i + 1]);
        }
        double slopefactor =
            splineValue(*param, startpole + degree, degree - 1, sd, flattenedknots);

        if (derivparam == polexat(i)) {
            result.dx = *weightat(i) * (wsum * slopefactor - wslopesum * factor);
        }
        else if (derivparam == poleyat(i)) {
            result.dy = *weightat(i) * (wsum * slopefactor - wslopesum * factor);
        }
        else if (derivparam == weightat(i)) {
            result.dx = degree
                * (factor * (xslopesum - wslopesum * (*polexat(i)))
                   - slopefactor * (xsum - wsum * (*polexat(i))));
            result.dy = degree
                * (factor * (yslopesum - wslopesum * (*poleyat(i)))
                   - slopefactor * (ysum - wsum * (*poleyat(i))));
        }
        break;
    }

    // the curve parameter being used by the constraint is not known to the geometry (there can be
    // many tangent constraints on the same curve after all). Assume that this is the param
    // provided.
    if (derivparam != param) {
        return result.rotate90ccw();
    }

    // derivparam == param now. Done this way just to reduce "cognitive complexity".
    VEC_D sd(numpoints - 1), ssd(numpoints - 2);
    for (size_t i = 1; i < numpoints; ++i) {
        sd[i - 1] = (*weightat(i) - *weightat(i - 1))
            / (flattenedknots[startpole + i + degree] - flattenedknots[startpole + i]);
    }
    for (size_t i = 1; i < numpoints - 1; ++i) {
        ssd[i - 1] = (sd[i] - sd[i - 1])
            / (flattenedknots[startpole + i + degree] - flattenedknots[startpole + i]);
    }
    double wslopeslopesum = degree * (degree - 1)
        * BSpline::splineValue(*param, startpole + degree, degree - 2, ssd, flattenedknots);

    for (size_t i = 1; i < numpoints; ++i) {
        sd[i - 1] = (*polexat(i) * *weightat(i) - *polexat(i - 1) * *weightat(i - 1))
            / (flattenedknots[startpole + i + degree] - flattenedknots[startpole + i]);
    }
    for (size_t i = 1; i < numpoints - 1; ++i) {
        ssd[i - 1] = (sd[i] - sd[i - 1])
            / (flattenedknots[startpole + i + degree] - flattenedknots[startpole + i]);
    }
    double xslopeslopesum = degree * (degree - 1)
        * BSpline::splineValue(*param, startpole + degree, degree - 2, ssd, flattenedknots);

    for (size_t i = 1; i < numpoints; ++i) {
        sd[i - 1] = (*poleyat(i) * *weightat(i) - *poleyat(i - 1) * *weightat(i - 1))
            / (flattenedknots[startpole + i + degree] - flattenedknots[startpole + i]);
    }
    for (size_t i = 1; i < numpoints - 1; ++i) {
        ssd[i - 1] = (sd[i] - sd[i - 1])
            / (flattenedknots[startpole + i + degree] - flattenedknots[startpole + i]);
    }
    double yslopeslopesum = degree * (degree - 1)
        * BSpline::splineValue(*param, startpole + degree, degree - 2, ssd, flattenedknots);

    result.dx = wsum * xslopeslopesum - wslopeslopesum * xsum;
    result.dy = wsum * yslopeslopesum - wslopeslopesum * ysum;

    return result.rotate90ccw();
}

DeriVector2 BSpline::Value(double u, double /*du*/, const double* /*derivparam*/) const
{
    // TODO: is there any advantage in making this a `static`?
    size_t startpole = 0;
    for (size_t j = 1; j < mult.size() && *(knots[j]) <= u; ++j) {
        startpole += mult[j];
    }
    if (!periodic && startpole >= poles.size()) {
        startpole = poles.size() - degree - 1;
    }

    // double xsum = 0., xslopesum = 0.;
    // double ysum = 0., yslopesum = 0.;
    // double wsum = 0., wslopesum = 0.;

    auto polexat = [&](size_t i) {
        return poles[(startpole + i) % poles.size()].x;
    };
    auto poleyat = [&](size_t i) {
        return poles[(startpole + i) % poles.size()].y;
    };
    auto weightat = [&](size_t i) {
        return weights[(startpole + i) % weights.size()];
    };

    size_t numpoints = degree + 1;
    // Tangent vector
    // This should in principle be identical to error gradient wrt curve parameter in
    // point-on-object
    VEC_D d(numpoints);
    for (size_t i = 0; i < numpoints; ++i) {
        d[i] = *polexat(i) * *weightat(i);
    }
    double xsum = splineValue(u, startpole + degree, degree, d, flattenedknots);
    for (size_t i = 0; i < numpoints; ++i) {
        d[i] = *poleyat(i) * *weightat(i);
    }
    double ysum = splineValue(u, startpole + degree, degree, d, flattenedknots);
    for (size_t i = 0; i < numpoints; ++i) {
        d[i] = *weightat(i);
    }
    double wsum = splineValue(u, startpole + degree, degree, d, flattenedknots);

    d.resize(numpoints - 1);
    for (size_t i = 1; i < numpoints; ++i) {
        d[i - 1] = (*polexat(i) * *weightat(i) - *polexat(i - 1) * *weightat(i - 1))
            / (flattenedknots[startpole + i + degree] - flattenedknots[startpole + i]);
    }
    double xslopesum = degree * splineValue(u, startpole + degree, degree - 1, d, flattenedknots);
    for (size_t i = 1; i < numpoints; ++i) {
        d[i - 1] = (*poleyat(i) * *weightat(i) - *poleyat(i - 1) * *weightat(i - 1))
            / (flattenedknots[startpole + i + degree] - flattenedknots[startpole + i]);
    }
    double yslopesum = degree * splineValue(u, startpole + degree, degree - 1, d, flattenedknots);
    for (size_t i = 1; i < numpoints; ++i) {
        d[i - 1] = (*weightat(i) - *weightat(i - 1))
            / (flattenedknots[startpole + i + degree] - flattenedknots[startpole + i]);
    }
    double wslopesum = degree * splineValue(u, startpole + degree, degree - 1, d, flattenedknots);

    return {xsum / wsum,
            ysum / wsum,
            (wsum * xslopesum - wslopesum * xsum) / wsum / wsum,
            (wsum * yslopesum - wslopesum * ysum) / wsum / wsum};
}

void BSpline::valueHomogenous(const double u,
                              double* xw,
                              double* yw,
                              double* w,
                              double* dxwdu,
                              double* dywdu,
                              double* dwdu) const
{
    // TODO: is there any advantage in making this a `static`?
    size_t startpole = 0;
    for (size_t j = 1; j < mult.size() && *(knots[j]) <= u; ++j) {
        startpole += mult[j];
    }
    if (!periodic && startpole >= poles.size()) {
        startpole = poles.size() - degree - 1;
    }

    auto polexat = [&](size_t i) {
        return poles[(startpole + i) % poles.size()].x;
    };
    auto poleyat = [&](size_t i) {
        return poles[(startpole + i) % poles.size()].y;
    };
    auto weightat = [&](size_t i) {
        return weights[(startpole + i) % weights.size()];
    };

    size_t numpoints = degree + 1;
    VEC_D d(numpoints);
    for (size_t i = 0; i < numpoints; ++i) {
        d[i] = *polexat(i) * *weightat(i);
    }
    *xw = BSpline::splineValue(u, startpole + degree, degree, d, flattenedknots);
    for (size_t i = 0; i < numpoints; ++i) {
        d[i] = *poleyat(i) * *weightat(i);
    }
    *yw = BSpline::splineValue(u, startpole + degree, degree, d, flattenedknots);
    for (size_t i = 0; i < numpoints; ++i) {
        d[i] = *weightat(i);
    }
    *w = BSpline::splineValue(u, startpole + degree, degree, d, flattenedknots);

    d.resize(numpoints - 1);
    for (size_t i = 1; i < numpoints; ++i) {
        d[i - 1] = (*polexat(i) * *weightat(i) - *polexat(i - 1) * *weightat(i - 1))
            / (flattenedknots[startpole + i + degree] - flattenedknots[startpole + i]);
    }
    *dxwdu = degree * BSpline::splineValue(u, startpole + degree, degree - 1, d, flattenedknots);
    for (size_t i = 1; i < numpoints; ++i) {
        d[i - 1] = (*poleyat(i) * *weightat(i) - *poleyat(i - 1) * *weightat(i - 1))
            / (flattenedknots[startpole + i + degree] - flattenedknots[startpole + i]);
    }
    *dywdu = degree * BSpline::splineValue(u, startpole + degree, degree - 1, d, flattenedknots);
    for (size_t i = 1; i < numpoints; ++i) {
        d[i - 1] = (*weightat(i) - *weightat(i - 1))
            / (flattenedknots[startpole + i + degree] - flattenedknots[startpole + i]);
    }
    *dwdu = degree * BSpline::splineValue(u, startpole + degree, degree - 1, d, flattenedknots);
}

int BSpline::PushOwnParams(VEC_pD& pvec)
{
    std::size_t cnt = 0;

    for (const auto& pole : poles) {
        pvec.push_back(pole.x);
        pvec.push_back(pole.y);
    }

    cnt = cnt + poles.size() * 2;

    pvec.insert(pvec.end(), weights.begin(), weights.end());
    cnt = cnt + weights.size();

    pvec.insert(pvec.end(), knots.begin(), knots.end());
    cnt = cnt + knots.size();

    pvec.push_back(start.x);
    cnt++;
    pvec.push_back(start.y);
    cnt++;
    pvec.push_back(end.x);
    cnt++;
    pvec.push_back(end.y);
    cnt++;

    return static_cast<int>(cnt);
}

void BSpline::ReconstructOnNewPvec(VEC_pD& pvec, int& cnt)
{
    for (auto& pole : poles) {
        pole.x = pvec[cnt];
        cnt++;
        pole.y = pvec[cnt];
        cnt++;
    }

    for (auto& weight : weights) {
        weight = pvec[cnt];
        cnt++;
    }

    for (auto& knot : knots) {
        knot = pvec[cnt];
        cnt++;
    }

    start.x = pvec[cnt];
    cnt++;
    start.y = pvec[cnt];
    cnt++;
    end.x = pvec[cnt];
    cnt++;
    end.y = pvec[cnt];
    cnt++;
}

BSpline* BSpline::Copy()
{
    return new BSpline(*this);
}

double BSpline::getLinCombFactor(double x, size_t k, size_t i, unsigned int p)
{
    // Adapted to C++ from the python implementation in the Wikipedia page for de Boor algorithm
    // https://en.wikipedia.org/wiki/De_Boor%27s_algorithm.

    // FIXME: This should probably be guaranteed by now, and done somewhere else
    // To elaborate: `flattenedknots` should be set up as soon as `knots`
    // and `mult` have been defined after creating the B-spline.
    // However, in the future the values of `knots` could go into the solver
    // as well, when alternatives may be needed to keep `flattenedknots` updated.
    // Slightly more detailed discussion here:
    // https://github.com/FreeCAD/FreeCAD/pull/7484#discussion_r1020858392
    if (flattenedknots.empty()) {
        setupFlattenedKnots();
    }

    std::vector d(p + 1, 0.0);
    // Ensure this is within range
    int idxOfPole = static_cast<int>(i) + p - static_cast<int>(k);
    if (idxOfPole < 0 || idxOfPole > static_cast<int>(p)) {
        return 0.0;
    }
    d[idxOfPole] = 1.0;

    for (size_t r = 1; r < p + 1; ++r) {
        for (size_t j = p; j > r - 1; --j) {
            double alpha = (x - flattenedknots[j + k - p])
                / (flattenedknots[j + 1 + k - r] - flattenedknots[j + k - p]);
            d[j] = (1.0 - alpha) * d[j - 1] + alpha * d[j];
        }
    }

    return d[p];
}

double BSpline::splineValue(double x, size_t k, unsigned int p, VEC_D& d, const VEC_D& flatknots)
{
    for (size_t r = 1; r < p + 1; ++r) {
        for (size_t j = p; j > r - 1; --j) {
            double alpha =
                (x - flatknots[j + k - p]) / (flatknots[j + 1 + k - r] - flatknots[j + k - p]);
            d[j] = (1.0 - alpha) * d[j - 1] + alpha * d[j];
        }
    }

    return p < d.size() ? d[p] : 0.0;
}

void BSpline::setupFlattenedKnots()
{
    flattenedknots.clear();

    for (size_t i = 0; i < knots.size(); ++i) {
        flattenedknots.insert(flattenedknots.end(), mult[i], *knots[i]);
    }

    // Adjust for periodic: see OCC documentation for explanation
    if (periodic) {
        double period = *knots.back() - *knots.front();
        int c = degree + 1 - mult[0];  // number of knots to pad

        // Add capacity so that iterators remain valid
        flattenedknots.reserve(flattenedknots.size() + 2 * c);

        // get iterators first for convenience
        auto frontStart = flattenedknots.end() - mult.back() - c;
        auto frontEnd = flattenedknots.end() - mult.back();
        auto backStart = flattenedknots.begin() + mult.front();
        auto backEnd = flattenedknots.begin() + mult.front() + c;

        // creating new vectors because above iterators can become invalidated upon insert
        std::vector<double> frontNew(frontStart, frontEnd);
        std::vector<double> backNew(backStart, backEnd);

        flattenedknots.insert(flattenedknots.end(), backNew.begin(), backNew.end());
        flattenedknots.insert(flattenedknots.begin(), frontNew.begin(), frontNew.end());

        for (int i = 0; i < c; ++i) {
            *(flattenedknots.begin() + i) -= period;
            *(flattenedknots.end() - 1 - i) += period;
        }
    }
}

}  // namespace GCS
