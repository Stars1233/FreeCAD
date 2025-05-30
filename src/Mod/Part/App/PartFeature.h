/***************************************************************************
 *   Copyright (c) 2002 Jürgen Riegel <juergen.riegel@web.de>              *
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

#ifndef PART_FEATURE_H
#define PART_FEATURE_H

#include <App/FeaturePython.h>
#include <App/GeoFeature.h>
#include <Mod/Material/App/PropertyMaterial.h>
#include <Mod/Part/PartGlobal.h>
#include <Base/Bitmask.h>

#include <TopoDS_Face.hxx>

#include "PropertyTopoShape.h"


class gp_Dir;
class BRepBuilderAPI_MakeShape;

namespace Data
{
struct HistoryItem;
}

namespace Part
{

enum class ShapeOption
{
    NoFlag = 0,
    NeedSubElement = 1,
    ResolveLink = 2,
    Transform = 4,
    NoElementMap = 8,
    DontSimplifyCompound = 16
};
using ShapeOptions = Base::Flags<ShapeOption>;


class PartFeaturePy;

/** Base class of all shape feature classes in FreeCAD
 */
class PartExport Feature : public App::GeoFeature
{
    PROPERTY_HEADER_WITH_OVERRIDE(Part::Feature);

public:
    /// Constructor
    Feature();
    ~Feature() override;

    PropertyPartShape Shape;
    Materials::PropertyMaterial ShapeMaterial;

    /** @name methods override feature */
    //@{
    short mustExecute() const override;
    //@}

    /// returns the type name of the ViewProvider
    const char* getViewProviderName() const override;
    const App::PropertyComplexGeoData* getPropertyOfGeometry() const override;

    PyObject* getPyObject() override;

    App::ElementNamePair getElementName(
            const char *name, ElementNameType type=Normal) const override;

    static std::list<Data::HistoryItem> getElementHistory(App::DocumentObject *obj,
                                                          const char *name, bool recursive=true, bool sameType=false);

    static QVector<Data::MappedElement>
    getRelatedElements(App::DocumentObject* obj,
                       const char* name,
                       HistoryTraceType sameType = HistoryTraceType::followTypeChange,
                       bool withCache = true);

    /** Obtain the element name from a feature based of the element name of its source feature
     *
     * @param obj: current feature
     * @param subname: sub-object/element reference
     * @param src: source feature
     * @param srcSub: sub-object/element reference of the source
     * @param single: if true, then return upon first match is found, or else
     *                return all matches. Multiple matches are possible for
     *                compound of multiple instances of the same source shape.
     *
     * @return Return a vector of pair of new style and old style element names.
     */
    static QVector<Data::MappedElement>
    getElementFromSource(App::DocumentObject *obj,
                         const char *subname,
                         App::DocumentObject *src,
                         const char *srcSub,
                         bool single = false);

    TopLoc_Location getLocation() const;

    DocumentObject *getSubObject(const char *subname, PyObject **pyObj,
            Base::Matrix4D *mat, bool transform, int depth) const override;

    App::Material getMaterialAppearance() const override;
    void setMaterialAppearance(const App::Material& material) override;

    /** Convenience function to extract shape from fully qualified subname
     *
     * @param obj: the parent object
     *
     * @param subname: dot separated full qualified subname
     *
     * @param needSubElement: whether to ignore the non-object subelement
     * reference inside \c subname
     *
     * @param pmat: used as current transformation on input, and return the
     * accumulated transformation on output
     *
     * @param owner: return the owner of the shape returned
     *
     * @param resolveLink: if true, resolve link(s) of the returned 'owner'
     * by calling its getLinkedObject(true) function
     *
     * @param transform: if true, apply obj's transformation. Set to false
     * if pmat already include obj's transformation matrix.
     */
    static TopoDS_Shape getShape(const App::DocumentObject *obj, ShapeOptions options,
            const char *subname=nullptr, Base::Matrix4D *pmat=nullptr,
            App::DocumentObject **owner=nullptr);

    static TopoShape getTopoShape(const App::DocumentObject* obj, ShapeOptions options,
                                    const char* subname=nullptr, Base::Matrix4D* pmat=nullptr, 
                                    App::DocumentObject**owner=nullptr);

    static TopoShape simplifyCompound(TopoShape compoundShape);
    static void clearShapeCache();

    static App::DocumentObject *getShapeOwner(const App::DocumentObject *obj, const char *subname=nullptr);

    static bool hasShapeOwner(const App::DocumentObject *obj, const char *subname=nullptr) {
        auto owner = getShapeOwner(obj,subname);
        return owner && owner->isDerivedFrom(getClassTypeId());
    }

    static Feature*
    create(const TopoShape& shape, const char* name = nullptr, App::Document* document = nullptr);

    static bool isElementMappingDisabled(App::PropertyContainer *container);

    bool getCameraAlignmentDirection(Base::Vector3d &directionZ, Base::Vector3d &directionX, const char *subname) const override;

    static void guessNewLink(std::string &replacementName, DocumentObject *base, const char *oldLink);

    const std::vector<std::string>& searchElementCache(const std::string &element,
                                                       Data::SearchOptions options = Data::SearchOption::CheckGeometry,
                                                       double tol = 1e-7,
                                                       double atol = 1e-10) const override;
protected:
    /// recompute only this object
    App::DocumentObjectExecReturn *recompute() override;
    /// recalculate the feature
    App::DocumentObjectExecReturn *execute() override;
    void onBeforeChange(const App::Property* prop) override;
    void onChanged(const App::Property* prop) override;
    void onDocumentRestored() override;

    void copyMaterial(Feature* feature);
    void copyMaterial(App::DocumentObject* link);

    void registerElementCache(const std::string &prefix, PropertyPartShape *prop);

    /** Helper function to obtain mapped and indexed element name from a shape
     * @params shape: source shape
     * @param name: the input name, can be either mapped or indexed name
     * @return Returns both the indexed and mapped name
     *
     * If the 'name' referencing a non-primary shape type, i.e. not
     * Vertex/Edge/Face, this function will auto generate a name from primary
     * sub-shapes.
     */
    App::ElementNamePair getExportElementName(TopoShape shape, const char *name) const;

    /**
     * Build a history of changes
     * MakeShape: The operation that created the changes, e.g. FCBRepAlgoAPI_Common
     * type: The type of object we are interested in, e.g. TopAbs_FACE
     * newS: The new shape that was created by the operation
     * oldS: The original shape prior to the operation
     */
    ShapeHistory buildHistory(BRepBuilderAPI_MakeShape&, TopAbs_ShapeEnum type,
        const TopoDS_Shape& newS, const TopoDS_Shape& oldS);
    ShapeHistory joinHistory(const ShapeHistory&, const ShapeHistory&);
private:
    struct ElementCache;
    std::map<std::string, ElementCache> _elementCache;
    std::vector<std::pair<std::string, PropertyPartShape*>> _elementCachePrefixMap;
};

class PartExport FilletBase : public Part::Feature
{
    PROPERTY_HEADER_WITH_OVERRIDE(Part::FilletBase);

public:
    FilletBase();

    App::PropertyLink   Base;
    PropertyFilletEdges Edges;
    App::PropertyLinkSub   EdgeLinks;

    short mustExecute() const override;
    App::DocumentObjectExecReturn* execute() override;
    void onUpdateElementReference(const App::Property* prop) override;

protected:
    void onDocumentRestored() override;
    void onChanged(const App::Property *) override;
    void syncEdgeLink();
};

using FeaturePython = App::FeaturePythonT<Feature>;


/** Base class of all shape feature classes in FreeCAD
 */
class PartExport FeatureExt : public Feature
{
    PROPERTY_HEADER_WITH_OVERRIDE(Part::FeatureExt);

public:
    const char* getViewProviderName() const override {
        return "PartGui::ViewProviderPartExt";
    }
};

// Utility methods
/**
 * Find all faces cut by a line through the centre of gravity of a given face
 * Useful for the "up to face" options to pocket or pad
 */
// TODO: Toponaming April 2024 Deprecated in favor of TopoShape method.  Remove when possible.
struct cutFaces {
    TopoDS_Face face;
    double distsq;
};

// TODO: Toponaming April 2024 Deprecated in favor of TopoShape method.  Remove when possible.
PartExport
std::vector<cutFaces> findAllFacesCutBy(const TopoDS_Shape& shape,
                                        const TopoDS_Shape& face, const gp_Dir& dir);
struct cutTopoShapeFaces
{
    TopoShape face;
    double distsq;
};

PartExport std::vector<cutTopoShapeFaces>
findAllFacesCutBy(const TopoShape& shape, const TopoShape& face, const gp_Dir& dir);

/**
  * Check for intersection between the two shapes. Only solids are guaranteed to work properly
  * There are two modes:
  * 1. Bounding box check only - quick but inaccurate
  * 2. Bounding box check plus (if necessary) boolean operation - costly but accurate
  * Return true if the shapes intersect, false if they don't
  * The flag touch_is_intersection decides whether shapes touching at distance zero are regarded
  * as intersecting or not
  * 1. If set to true, a true check result means that a boolean fuse operation between the two shapes
  *    will return a single solid
  * 2. If set to false, a true check result means that a boolean common operation will return a
  *    valid solid
  * If there is any error in the boolean operations, the check always returns false
  */
PartExport
bool checkIntersection(const TopoDS_Shape& first, const TopoDS_Shape& second,
                       const bool quick, const bool touch_is_intersection);

} //namespace Part

ENABLE_BITMASK_OPERATORS(Part::ShapeOption)

#endif // PART_FEATURE_H

