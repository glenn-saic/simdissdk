/* -*- mode: c++ -*- */
/****************************************************************************
 *****                                                                  *****
 *****                   Classification: UNCLASSIFIED                   *****
 *****                    Classified By:                                *****
 *****                    Declassify On:                                *****
 *****                                                                  *****
 ****************************************************************************
 *
 *
 * Developed by: Naval Research Laboratory, Tactical Electronic Warfare Div.
 *               EW Modeling & Simulation, Code 5773
 *               4555 Overlook Ave.
 *               Washington, D.C. 20375-5339
 *
 * License for source code can be found at:
 * https://github.com/USNavalResearchLaboratory/simdissdk/blob/master/LICENSE.txt
 *
 * The U.S. Government retains all rights to use, duplicate, distribute,
 * disclose, or release this software.
 *
 */

#include <sstream>
#include "simCore/Calc/Angle.h"
#include "simCore/Calc/Math.h"
#include "simCore/Calc/Units.h"
#include "simCore/Common/SDKAssert.h"
#include "simCore/Common/Version.h"
#include "simCore/GOG/GogShape.h"
#include "simCore/GOG/Parser.h"

namespace {

// outlined shape optional field in GOG format
static const std::string OUTLINED_FIELD = "outline true\n";
// fillable shape optional fields in GOG format
static const std::string FILLABLE_FIELDS = OUTLINED_FIELD + "linewidth 4\n linecolor green\n linestyle dashed\n filled\n fillcolor yellow\n";
// circular shape optional fields in GOG format (in meters for testing)
static const std::string CIRCULAR_FIELDS = FILLABLE_FIELDS + " radius 1000.\n rangeunits m\n";
// point based shape optional field in GOG format
static const std::string POINTBASED_FIELDS = FILLABLE_FIELDS + " tessellate true\n lineprojection greatcircle\n";
// elliptical shape optional fields in GOG format
static const std::string ELLIPTICAL_FIELDS = CIRCULAR_FIELDS + " anglestart 10.\n angledeg 45.\n majoraxis 100.\n minoraxis 250.\n";
// height field in GOG format (in meters for testing)
static const std::string HEIGHT_FIELD = "height 180.\n altitudeunits m\n";
// points shape optional fields in GOG format
static const std::string POINTS_FIELDS = OUTLINED_FIELD + " pointsize 5\n linecolor magenta\n";

// return true if the specified positions are equal
bool comparePositions(const simCore::Vec3& pos1, const simCore::Vec3& pos2)
{
  return simCore::areEqual(pos1.x(), pos2.x()) && simCore::areEqual(pos1.y(), pos2.y()) && simCore::areEqual(pos1.z(), pos2.z());
}

// return true if all the positions in pos2 are in pos1
bool comparePositionVectors(const std::vector<simCore::Vec3>& pos1, const std::vector<simCore::Vec3>& pos2)
{
  size_t found = 0;
  for (simCore::Vec3 position : pos1)
  {
    for (simCore::Vec3 position2 : pos2)
    {
      if (comparePositions(position, position2))
        found++;
    }
  }
  return (found == pos1.size());
}

// test basic GOG format syntax checking
int testGeneralSyntax()
{
  int rv = 0;
  simCore::GOG::Parser parser;
  std::vector<simCore::GOG::GogShapePtr> shapes;

  // test file with missing end fails to create shape
  std::stringstream missingEnd;
  missingEnd << "start\n circle\n";
  parser.parse(missingEnd, shapes);
  rv += SDK_ASSERT(shapes.empty());
  shapes.clear();

  // test file with missing start fails to create shape
  std::stringstream missingStart;
  missingStart << "circle\n end\n";
  parser.parse(missingStart, shapes);
  rv += SDK_ASSERT(shapes.empty());
  shapes.clear();

  // test file with multiple keywords between start/end fails to create shape
  std::stringstream uncertainShape;
  uncertainShape << "start\n circle\n line\n centerlla 25.1 58.2 0.\n end\n";
  parser.parse(uncertainShape, shapes);
  rv += SDK_ASSERT(shapes.empty());
  shapes.clear();

  // test mixed case creates shapes
  std::stringstream mixedCaseCircle;
  mixedCaseCircle << "start\n CirCle\n centerLL 25.1 58.2\n END\n ";
  parser.parse(mixedCaseCircle, shapes);
  rv += SDK_ASSERT(!shapes.empty());
  if (!shapes.empty())
    rv += SDK_ASSERT(shapes.front()->shapeType() == simCore::GOG::GogShape::ShapeType::CIRCLE);
  shapes.clear();
  std::stringstream mixedCaseLine;
  mixedCaseLine << "StarT\n LINE\n ll 22.2 23.2\n LL 22.5 25.2\nenD\n";
  parser.parse(mixedCaseLine, shapes);
  rv += SDK_ASSERT(!shapes.empty());
  if (!shapes.empty())
    rv += SDK_ASSERT(shapes.front()->shapeType() == simCore::GOG::GogShape::ShapeType::LINE);
  shapes.clear();

  return rv;
}

int testBaseOptionalFieldsNotSet(const simCore::GOG::GogShape* shape)
{
  int rv = 0;
  std::string name;
  rv += SDK_ASSERT(shape->getName(name) != 0);
  bool draw = false;
  rv += SDK_ASSERT(shape->getIsDrawn(draw) != 0);
  rv += SDK_ASSERT(draw);
  bool depthBuffer = true;
  rv += SDK_ASSERT(shape->getIsDepthBufferActive(depthBuffer) != 0);
  rv += SDK_ASSERT(!depthBuffer);
  double altOffset = 10.;
  rv += SDK_ASSERT(shape->getAltitudeOffset(altOffset) != 0);
  rv += SDK_ASSERT(altOffset == 0.);
  simCore::GOG::GogShape::AltitudeMode mode = simCore::GOG::GogShape::AltitudeMode::CLAMP_TO_GROUND;
  rv += SDK_ASSERT(shape->getAltitudeMode(mode) != 0);
  rv += SDK_ASSERT(mode == simCore::GOG::GogShape::AltitudeMode::NONE);
  simCore::Vec3 refPos(25., 25., 25.);
  rv += SDK_ASSERT(shape->getReferencePosition(refPos) != 0);
  // default ref postiion is BSTUR
  rv += SDK_ASSERT(refPos == simCore::Vec3(simCore::DEG2RAD * 22.1194392, simCore::DEG2RAD * -159.9194988, 0.0));
  simCore::Vec3 scalar(10., 10., 10.);
  rv += SDK_ASSERT(shape->getScale(scalar) != 0);
  rv += SDK_ASSERT(scalar == simCore::Vec3(1., 1., 1.));
  bool followYaw = true;
  rv += SDK_ASSERT(shape->getIsFollowingYaw(followYaw) != 0);
  rv += SDK_ASSERT(!followYaw);
  bool followPitch = true;
  rv += SDK_ASSERT(shape->getIsFollowingPitch(followPitch) != 0);
  rv += SDK_ASSERT(!followPitch);
  bool followRoll = true;
  rv += SDK_ASSERT(shape->getIsFollowingRoll(followRoll) != 0);
  rv += SDK_ASSERT(!followRoll);
  double yawOffset = 10.;
  rv += SDK_ASSERT(shape->getYawOffset(yawOffset) != 0);
  rv += SDK_ASSERT(yawOffset == 0.);
  double pitchOffset = 10.;
  rv += SDK_ASSERT(shape->getPitchOffset(pitchOffset) != 0);
  rv += SDK_ASSERT(pitchOffset == 0.);
  double rollOffset = 10.;
  rv += SDK_ASSERT(shape->getPitchOffset(rollOffset) != 0);
  rv += SDK_ASSERT(rollOffset == 0.);

  return rv;
}

// test outlined shape's optional field is not set and returns default value
int testOutlinedOptionalFieldNotSet(const simCore::GOG::OutlinedShape* shape)
{
  int rv = testBaseOptionalFieldsNotSet(shape);
  bool outlined = false;
  rv += SDK_ASSERT(shape->getIsOutlined(outlined) != 0);
  rv += SDK_ASSERT(outlined);
  return rv;
}

// test that fillable shape's optional fields are not set and return default values
int testFillableOptionalFieldsNotSet(const simCore::GOG::FillableShape* shape)
{
  int rv = testOutlinedOptionalFieldNotSet(shape);
  int lineWidth = 0;
  rv += SDK_ASSERT(shape->getLineWidth(lineWidth) != 0);
  rv += SDK_ASSERT(lineWidth == 1);
  simCore::GOG::FillableShape::LineStyle style = simCore::GOG::FillableShape::LineStyle::DASHED;
  rv += SDK_ASSERT(shape->getLineStyle(style) != 0);
  rv += SDK_ASSERT(style == simCore::GOG::FillableShape::LineStyle::SOLID);
  simCore::GOG::GogShape::Color color(0, 255, 255, 0);
  rv += SDK_ASSERT(shape->getLineColor(color) != 0);
  rv += SDK_ASSERT(color == simCore::GOG::GogShape::Color());
  bool filled = true;
  rv += SDK_ASSERT(shape->getIsFilled(filled) != 0);
  rv += SDK_ASSERT(!filled);
  simCore::GOG::GogShape::Color fillColor(0, 255, 255, 0);
  rv += SDK_ASSERT(shape->getFillColor(fillColor) != 0);
  rv += SDK_ASSERT(fillColor == simCore::GOG::GogShape::Color());
  return rv;
}

// test the circular shape's required fields are set, and the optional fields are not
auto testCircularShapeMinimalFieldsFunc = [](const simCore::GOG::CircularShape* shape, const std::vector<simCore::Vec3>& positions) -> int
{
  int rv = testFillableOptionalFieldsNotSet(shape);
  const simCore::Vec3& center = shape->centerPosition();
  rv += SDK_ASSERT(comparePositions(center, positions.front()));
  double radius = 0.;
  // verify radius wasn't set
  rv += SDK_ASSERT(shape->getRadius(radius) == 1);
  // verify default value was returned
  rv += SDK_ASSERT(radius == 500.);

  return rv;
};

// test the orbit shape's required fields are set, and the optional fields are not
auto testOrbitShapeMinimalFieldsFunc = [](const simCore::GOG::Orbit* shape, const std::vector<simCore::Vec3>& positions) -> int
{
  // dev error, require 2 positions to test orbit
  assert(positions.size() == 2);
  int rv = testCircularShapeMinimalFieldsFunc(shape, positions);
  if (positions.size() > 1)
  {
    const simCore::Vec3& center2 = shape->centerPosition2();
    rv += SDK_ASSERT(comparePositions(center2, positions[1]));
  }
  return rv;
};

// test the cone shape's required fields are set, and the optional fields are not
auto testCircularHeightShapeMinimalFieldsFunc = [](const simCore::GOG::CircularHeightShape* shape, const std::vector<simCore::Vec3>& positions) -> int
{
  int rv = testCircularShapeMinimalFieldsFunc(shape, positions);
  double height = 0.;
  rv += SDK_ASSERT(shape->getHeight(height) != 0);
  rv += SDK_ASSERT(height == 500.);
  return rv;
};

// test the ellipsoid shape's required fields are set, and the optional fields are not
auto testEllipsoidShapeMinimalFieldsFunc = [](const simCore::GOG::Ellipsoid* shape, const std::vector<simCore::Vec3>& positions) -> int
{
  int rv = testCircularHeightShapeMinimalFieldsFunc(shape, positions);
  double majorAxis = 0.;
  rv += SDK_ASSERT(shape->getMajorAxis(majorAxis) != 0);
  rv += SDK_ASSERT(majorAxis == 1000.);
  double minorAxis = 0.;
  rv += SDK_ASSERT(shape->getMinorAxis(minorAxis) != 0);
  rv += SDK_ASSERT(minorAxis == 1000.);
  return rv;
};

// test elliptical shape's required fields are set, and the optional fields are not
auto testEllipticalShapeMinimalFieldsFunc = [](const simCore::GOG::EllipticalShape* shape, const std::vector<simCore::Vec3>& positions) -> int
{
  int rv = testCircularShapeMinimalFieldsFunc(shape, positions);
  double angleStart = 10.;
  rv += SDK_ASSERT(shape->getAngleStart(angleStart) != 0);
  rv += SDK_ASSERT(angleStart == 0.);
  double angleSweep = 10.;
  rv += SDK_ASSERT(shape->getAngleSweep(angleSweep) != 0);
  rv += SDK_ASSERT(angleSweep == 0.);
  double majorAxis = 10.;
  rv += SDK_ASSERT(shape->getMajorAxis(majorAxis) != 0);
  rv += SDK_ASSERT(majorAxis == 0.);
  double minorAxis = 10.;
  rv += SDK_ASSERT(shape->getMinorAxis(minorAxis) != 0);
  rv += SDK_ASSERT(minorAxis == 0.);
  return rv;
};

// test the point based shape's required fields are set, and the optional fields are not
auto testPointBasedShapeMinimalFieldsFunc = [](const simCore::GOG::PointBasedShape* shape, const std::vector<simCore::Vec3>& positions) -> int
{
  int rv = testFillableOptionalFieldsNotSet(shape);
  const std::vector<simCore::Vec3>& positionsOut = shape->points();
  rv += SDK_ASSERT(positions.size() == positionsOut.size());
  rv += SDK_ASSERT(comparePositionVectors(positions, positionsOut));

  // verify that tessellation has not been set
  simCore::GOG::PointBasedShape::TessellationStyle style = simCore::GOG::PointBasedShape::TessellationStyle::NONE;
  rv += SDK_ASSERT(shape->getTessellation(style) != 0);
  rv += SDK_ASSERT(style == simCore::GOG::PointBasedShape::TessellationStyle::NONE);
  return rv;
};

// test the points shape's required fields are set, and the optional fields are not
auto testPointsShapeMinimalFieldsFunc = [](const simCore::GOG::Points* shape, const std::vector<simCore::Vec3>& positions) -> int
{
  int rv = testOutlinedOptionalFieldNotSet(shape);
  const std::vector<simCore::Vec3>& positionsOut = shape->points();
  rv += SDK_ASSERT(positions.size() == positionsOut.size());
  rv += SDK_ASSERT(comparePositionVectors(positions, positionsOut));

  int pointSize = 0;
  rv += SDK_ASSERT(shape->getPointSize(pointSize) != 0);
  rv += SDK_ASSERT(pointSize == 1);
  simCore::GOG::GogShape::Color color(0, 255, 255, 0);
  rv += SDK_ASSERT(shape->getColor(color) != 0);
  rv += SDK_ASSERT(color == simCore::GOG::GogShape::Color());

  return rv;
};
// test that the specified gog string parses to the specified object, and calls the specified function with the shape and positions
template <typename ClassT, typename FunctionT>
int testShapePositionsFunction(const std::string& gog, const FunctionT& func, const std::vector<simCore::Vec3>& positions)
{
  simCore::GOG::Parser parser;
  std::vector<simCore::GOG::GogShapePtr> shapes;
  int rv = 0;

  std::stringstream gogStr;
  gogStr << gog;
  parser.parse(gogStr, shapes);
  rv += SDK_ASSERT(shapes.size() == 1);
  if (!shapes.empty())
  {
    ClassT* shape = dynamic_cast<ClassT*>(shapes.front().get());
    rv += SDK_ASSERT(shape != nullptr);
    if (shape)
      rv += func(shape, positions);
  }
  shapes.clear();
  return rv;
}

// test shapes with only minimum required fields set
int testMinimalShapes()
{
  int rv = 0;

  // ABSOLUTE

  // test circle
  std::vector<simCore::Vec3> centerPoint;
  centerPoint.push_back(simCore::Vec3(25.1 * simCore::DEG2RAD, 58.2 * simCore::DEG2RAD, 0.));
  rv += testShapePositionsFunction<simCore::GOG::Circle>("start\n circle\n centerlla 25.1 58.2 0.\n end\n", testCircularShapeMinimalFieldsFunc, centerPoint);
  // test sphere
  rv += testShapePositionsFunction<simCore::GOG::Sphere>("start\n sphere\n centerlla 25.1 58.2 0.\n end\n", testCircularShapeMinimalFieldsFunc, centerPoint);
  // test hemisphere
  rv += testShapePositionsFunction<simCore::GOG::Hemisphere>("start\n hemisphere\n centerlla 25.1 58.2 0.\n end\n", testCircularShapeMinimalFieldsFunc, centerPoint);
  // test ellipsoid
  rv += testShapePositionsFunction<simCore::GOG::Ellipsoid>("start\n ellipsoid\n centerlla 25.1 58.2 0.\n end\n", testEllipsoidShapeMinimalFieldsFunc, centerPoint);
  // test arc
  rv += testShapePositionsFunction<simCore::GOG::Arc>("start\n arc\n centerlla 25.1 58.2 0.\n end\n", testEllipticalShapeMinimalFieldsFunc, centerPoint);
  // test ellipse
  rv += testShapePositionsFunction<simCore::GOG::Ellipse>("start\n ellipse\n centerlla 25.1 58.2 0.\n end\n", testEllipticalShapeMinimalFieldsFunc, centerPoint);
  // test cylinder
  rv += testShapePositionsFunction<simCore::GOG::Cylinder>("start\n cylinder\n centerlla 25.1 58.2 0.\n end\n", testEllipticalShapeMinimalFieldsFunc, centerPoint);
  // test cone
  rv += testShapePositionsFunction<simCore::GOG::Cone>("start\n cone\n centerlla 25.1 58.2 0.\n end\n", testCircularHeightShapeMinimalFieldsFunc, centerPoint);

  // test orbit
  std::vector<simCore::Vec3> orbitCtrs;
  orbitCtrs.push_back(simCore::Vec3(24.4 * simCore::DEG2RAD, 43.2 * simCore::DEG2RAD, 0.));
  orbitCtrs.push_back(simCore::Vec3(24.1 * simCore::DEG2RAD, 43.5 * simCore::DEG2RAD, 0.));
  rv += testShapePositionsFunction<simCore::GOG::Orbit>("start\n orbit\n centerlla 24.4 43.2 0.0\n centerll2 24.1 43.5\n end\n", testOrbitShapeMinimalFieldsFunc, orbitCtrs);

  // test line
  std::vector<simCore::Vec3> linePoints;
  linePoints.push_back(simCore::Vec3(25.1 * simCore::DEG2RAD, 58.2 * simCore::DEG2RAD, 0.));
  linePoints.push_back(simCore::Vec3(26.2 * simCore::DEG2RAD, 58.3 * simCore::DEG2RAD, 0.));
  rv += testShapePositionsFunction<simCore::GOG::Line>("start\n line\n lla 25.1 58.2 0.\n lla 26.2 58.3 0.\n end\n", testPointBasedShapeMinimalFieldsFunc, linePoints);
  // test linesegs
  rv += testShapePositionsFunction<simCore::GOG::LineSegs>("start\n linesegs\n lla 25.1 58.2 0.\n lla 26.2 58.3 0.\n end\n", testPointBasedShapeMinimalFieldsFunc, linePoints);
  // test polygon
  linePoints.push_back(simCore::Vec3(26.2 * simCore::DEG2RAD, 57.9 * simCore::DEG2RAD, 0.));
  rv += testShapePositionsFunction<simCore::GOG::Polygon>("start\n poly\n lla 25.1 58.2 0.\n lla 26.2 58.3 0.\n lla 26.2 57.9 0.\n end\n", testPointBasedShapeMinimalFieldsFunc, linePoints);
  // test points
  rv += testShapePositionsFunction<simCore::GOG::Points>("start\n points\n lla 25.1 58.2 0.\n lla 26.2 58.3 0.\n lla 26.2 57.9 0.\n end\n", testPointsShapeMinimalFieldsFunc, linePoints);

  // RELATIVE

  // test circle
  std::vector<simCore::Vec3> xyzPoint;
  xyzPoint.push_back(simCore::Vec3(15.2, 20., 10.));
  rv += testShapePositionsFunction<simCore::GOG::Circle>("start\n circle\n centerxyz 15.2 20. 10.\n rangeunits m\n altitudeunits m\n end\n", testCircularShapeMinimalFieldsFunc, xyzPoint);
  // test sphere
  rv += testShapePositionsFunction<simCore::GOG::Sphere>("start\n sphere\n centerxyz 15.2 20. 10.\n rangeunits m\n altitudeunits m\n end\n", testCircularShapeMinimalFieldsFunc, xyzPoint);
  // test hemisphere
  rv += testShapePositionsFunction<simCore::GOG::Hemisphere>("start\n hemisphere\n centerxyz 15.2 20. 10.\n rangeunits m\n altitudeunits m\n end\n", testCircularShapeMinimalFieldsFunc, xyzPoint);
  // test ellipsoid
  rv += testShapePositionsFunction<simCore::GOG::Ellipsoid>("start\n ellipsoid\n centerxyz 15.2 20. 10.\n rangeunits m\n altitudeunits m\n end\n", testEllipsoidShapeMinimalFieldsFunc, xyzPoint);
  // test arc
  rv += testShapePositionsFunction<simCore::GOG::Arc>("start\n arc\n centerxyz 15.2 20. 10.\n rangeunits m\n altitudeunits m\n end\n", testEllipticalShapeMinimalFieldsFunc, xyzPoint);
  // test ellipse
  rv += testShapePositionsFunction<simCore::GOG::Ellipse>("start\n ellipse\n centerxyz 15.2 20. 10.\n rangeunits m\n altitudeunits m\n end\n", testEllipticalShapeMinimalFieldsFunc, xyzPoint);
  // test cylinder
  rv += testShapePositionsFunction<simCore::GOG::Cylinder>("start\n cylinder\n centerxyz 15.2 20. 10.\n rangeunits m\n altitudeunits m\n end\n", testEllipticalShapeMinimalFieldsFunc, xyzPoint);
  // test cone
  rv += testShapePositionsFunction<simCore::GOG::Cone>("start\n cone\n centerxyz 15.2 20. 10.\n rangeunits m\n altitudeunits m\n end\n", testCircularHeightShapeMinimalFieldsFunc, xyzPoint);

  // test orbit
  std::vector<simCore::Vec3> orbitXyzCtrs;
  orbitXyzCtrs.push_back(simCore::Vec3(24.4, 43.2, 0.));
  orbitXyzCtrs.push_back(simCore::Vec3(24.1, 43.5, 0.));
  rv += testShapePositionsFunction<simCore::GOG::Orbit>("start\n orbit\n centerxyz 24.4 43.2 0.0\n centerxy2 24.1 43.5\n rangeunits m\n altitudeunits m\n end\n", testOrbitShapeMinimalFieldsFunc, orbitXyzCtrs);
 
  // test line
  std::vector<simCore::Vec3> lineXyzPoints;
  lineXyzPoints.push_back(simCore::Vec3(10., 10., 10.));
  lineXyzPoints.push_back(simCore::Vec3(100.0, -500., 10.));
  rv += testShapePositionsFunction<simCore::GOG::Line>("start\n line\n xyz 10. 10. 10.\n xyz 100. -500. 10.\n  rangeunits m\n altitudeunits m\n end\n", testPointBasedShapeMinimalFieldsFunc, lineXyzPoints);
  // test linesegs
  rv += testShapePositionsFunction<simCore::GOG::LineSegs>("start\n linesegs\n xyz 10. 10. 10.\n xyz 100. -500. 10.\n  rangeunits m\n altitudeunits m\n end\n", testPointBasedShapeMinimalFieldsFunc, lineXyzPoints);
  // test polygon
  lineXyzPoints.push_back(simCore::Vec3(-500., 50., 0.));
  rv += testShapePositionsFunction<simCore::GOG::Polygon>("start\n poly\n xyz 10. 10. 10.\n xyz 100. -500. 10.\n xyz -500. 50. 0.\n  rangeunits m\n altitudeunits m\n end\n", testPointBasedShapeMinimalFieldsFunc, lineXyzPoints);
  // test points
  rv += testShapePositionsFunction<simCore::GOG::Points>("start\n points\n xyz 10. 10. 10.\n xyz 100. -500. 10.\n xyz -500. 50. 0.\n  rangeunits m\n altitudeunits m\n end\n", testPointsShapeMinimalFieldsFunc, lineXyzPoints);

  return rv;
}

// test that the shape's optional field matches the pre-defined test fields from OUTLINED_FIELD
int testOutlinedField(const simCore::GOG::OutlinedShape* shape)
{
  int rv = 0;

  bool outlined = false;
  rv += SDK_ASSERT(shape->getIsOutlined(outlined) == 0);
  rv += SDK_ASSERT(outlined);

  return rv;
}

// test that the shape's optional fields match the pre-defined test fields from FILLABLE_FIELDS
auto testFillableShapeOptionalFieldsFunc = [](const simCore::GOG::FillableShape* shape) -> int
{
  int rv = testOutlinedField(shape);

  int lineWidth = 0;
  rv += SDK_ASSERT(shape->getLineWidth(lineWidth) == 0);
  rv += SDK_ASSERT(lineWidth == 4);

  simCore::GOG::FillableShape::LineStyle style = simCore::GOG::FillableShape::LineStyle::SOLID;
  rv += SDK_ASSERT(shape->getLineStyle(style) == 0);
  rv += SDK_ASSERT(style == simCore::GOG::FillableShape::LineStyle::DASHED);

  simCore::GOG::GogShape::Color lineColor;
  rv += SDK_ASSERT(shape->getLineColor(lineColor) == 0);
  rv += SDK_ASSERT(lineColor == simCore::GOG::GogShape::Color(0, 255, 0, 255));

  bool filled = false;
  rv += SDK_ASSERT(shape->getIsFilled(filled) == 0);
  rv += SDK_ASSERT(filled);

  simCore::GOG::GogShape::Color fillColor;
  rv += SDK_ASSERT(shape->getFillColor(fillColor) == 0);
  rv += SDK_ASSERT(fillColor == simCore::GOG::GogShape::Color(255, 255, 0, 255));

  return rv;
};

// test that the shape's optional fields match the pre-defined test fields from CIRCULAR_FIELDS
auto testCircularShapeOptionalFieldsFunc = [](const simCore::GOG::CircularShape* shape) -> int
{
  int rv = testFillableShapeOptionalFieldsFunc(shape);
  double radius = 0.;
  rv += SDK_ASSERT(shape->getRadius(radius) == 0);
  rv += SDK_ASSERT(radius == 1000.);
  return rv;
};

// test the shape's optional fields match the pre-defined test fields from POINTBASED_FIELDS
auto testPointBasedShapeOptionalFieldsFunc = [](const simCore::GOG::PointBasedShape* shape) -> int
{
  int rv = testFillableShapeOptionalFieldsFunc(shape);
  simCore::GOG::PointBasedShape::TessellationStyle style = simCore::GOG::PointBasedShape::TessellationStyle::NONE;
  rv += SDK_ASSERT(shape->getTessellation(style) == 0);
  rv += SDK_ASSERT(style == simCore::GOG::PointBasedShape::TessellationStyle::GREAT_CIRCLE);
  return rv;
};

// test the point shape's optional fields match the pre-defined test fields from POINTS_FIELDS
auto testPointsOptionalFieldsFunc = [](const simCore::GOG::Points* shape) -> int
{
  int rv = testOutlinedField(shape);

  int pointSize = 0;
  rv += SDK_ASSERT(shape->getPointSize(pointSize) == 0);
  rv += SDK_ASSERT(pointSize == 5);
  simCore::GOG::GogShape::Color color;
  rv += SDK_ASSERT(shape->getColor(color) == 0);
  rv += SDK_ASSERT(color == simCore::GOG::GogShape::Color(192, 0, 192, 255));

  return rv;
};

// test the elliptical shape's optional fields match the pre-defined test fields from ELLIPTICAL_FIELDS
auto testEllipticalShapeOptionalFieldsFunc = [](const simCore::GOG::EllipticalShape* shape) -> int
{
  int rv = testCircularShapeOptionalFieldsFunc(shape);
  double angleStart = 0.;
  rv += SDK_ASSERT(shape->getAngleStart(angleStart) == 0);
  rv += SDK_ASSERT(simCore::areEqual(angleStart * simCore::RAD2DEG, 10.));
  double angleSweep = 0.;
  rv += SDK_ASSERT(shape->getAngleSweep(angleSweep) == 0);
  rv += SDK_ASSERT(simCore::areEqual(angleSweep * simCore::RAD2DEG, 45.));
  double majorAxis = 0.;
  rv += SDK_ASSERT(shape->getMajorAxis(majorAxis) == 0);
  rv += SDK_ASSERT(majorAxis == 100.);
  double minorAxis = 0.;
  rv += SDK_ASSERT(shape->getMinorAxis(minorAxis) == 0);
  rv += SDK_ASSERT(minorAxis == 250.);
  return rv;
};

// test the cylinder shape's optional height field matches the pre-defined test field from HEIGHT_FIELD
auto testCircularHeightShapeOptionalFieldsFunc = [](const simCore::GOG::CircularHeightShape* shape) -> int
{
  int rv = testCircularShapeOptionalFieldsFunc(shape);
  double height = 0.;
  rv += SDK_ASSERT(shape->getHeight(height) == 0);
  rv += SDK_ASSERT(height == 180.);
  return rv;
};

// test the cylinder shape's optional height field matches the pre-defined test field from HEIGHT_FIELD
auto testCylinderShapeOptionalFieldsFunc = [](const simCore::GOG::Cylinder* shape) -> int
{
  int rv = testEllipticalShapeOptionalFieldsFunc(shape);
  double height = 0.;
  rv += SDK_ASSERT(shape->getHeight(height) == 0);
  rv += SDK_ASSERT(height == 180.);
  return rv;
};

// test the ellipsoid shape's optional fields match the pre-defined test field from ELLIPTICAL_FIELDS (ignores anglestart and angledeg)
auto testEllipsoidShapeOptionalFieldsFunc = [](const simCore::GOG::Ellipsoid* shape) -> int
{
  int rv = testCircularHeightShapeOptionalFieldsFunc(shape);
  double majorAxis = 0.;
  rv += SDK_ASSERT(shape->getMajorAxis(majorAxis) == 0);
  rv += SDK_ASSERT(majorAxis == 100.);
  double minorAxis = 0.;
  rv += SDK_ASSERT(shape->getMinorAxis(minorAxis) == 0);
  rv += SDK_ASSERT(minorAxis == 250.);
  return rv;
};

// test that the specified gog string parses to the specified object, and that its optional fields match the pre-defined test fields
template <typename ClassT, typename FunctionT>
int testShapeFunction(const std::string& gog, const FunctionT& func)
{
  simCore::GOG::Parser parser;
  std::vector<simCore::GOG::GogShapePtr> shapes;
  int rv = 0;

  std::stringstream gogStr;
  gogStr << gog;
  parser.parse(gogStr, shapes);
  rv += SDK_ASSERT(shapes.size() == 1);
  if (!shapes.empty())
  {
    ClassT* shape = dynamic_cast<ClassT*>(shapes.front().get());
    rv += SDK_ASSERT(shape != nullptr);
    if (shape)
      rv += func(shape);
  }
  shapes.clear();
  return rv;
}

// test shapes with all fields set
int testShapesOptionalFields()
{
  int rv = 0;

  // test circle
  rv += testShapeFunction<simCore::GOG::Circle>("start\n circle\n centerlla 24.4 43.2 0.0\n" + CIRCULAR_FIELDS + " end\n",  testCircularShapeOptionalFieldsFunc);
  // test sphere
  rv += testShapeFunction<simCore::GOG::Sphere>("start\n sphere\n centerlla 24.4 43.2 0.0\n" + CIRCULAR_FIELDS + " end\n", testCircularShapeOptionalFieldsFunc);
  // test hemisphere
  rv += testShapeFunction<simCore::GOG::Hemisphere>("start\n hemisphere\n centerlla 24.4 43.2 0.0\n" + CIRCULAR_FIELDS + " end\n", testCircularShapeOptionalFieldsFunc);
  // test orbit
  rv += testShapeFunction<simCore::GOG::Orbit>("start\n orbit\n centerlla 24.4 43.2 0.0\n centerll2 24.1 43.5\n" + CIRCULAR_FIELDS + " end\n", testCircularShapeOptionalFieldsFunc);

  // test line
  rv += testShapeFunction<simCore::GOG::Line>("start\n line\n lla 25.1 58.2 0.\n lla 26.2 58.3 0.\n" + POINTBASED_FIELDS + " end\n", testPointBasedShapeOptionalFieldsFunc);
  // test linesegs
  rv += testShapeFunction<simCore::GOG::LineSegs>("start\n linesegs\n lla 25.1 58.2 0.\n lla 26.2 58.3 0.\n" + POINTBASED_FIELDS + " end\n", testPointBasedShapeOptionalFieldsFunc);
  // test polygon
  rv += testShapeFunction<simCore::GOG::Polygon>("start\n poly\n lla 25.1 58.2 0.\n lla 26.2 58.3 0.\n lla 26.2 57.9 0.\n" + POINTBASED_FIELDS + " end\n", testPointBasedShapeOptionalFieldsFunc);
  // test points
  rv += testShapeFunction<simCore::GOG::Points>("start\n points\n lla 25.1 58.2 0.\n lla 26.2 58.3 0.\n" + POINTS_FIELDS + " end\n", testPointsOptionalFieldsFunc);

  // test arc
  rv += testShapeFunction<simCore::GOG::Arc>("start\n arc\n centerlla 24.4 43.2 0.0\n" + ELLIPTICAL_FIELDS + "end\n", testEllipticalShapeOptionalFieldsFunc);
  // test ellipse
  rv += testShapeFunction<simCore::GOG::Ellipse>("start\n ellipse\n centerlla 24.4 43.2 0.0\n" + ELLIPTICAL_FIELDS + "end\n", testEllipticalShapeOptionalFieldsFunc);
  // test cylinder
  rv += testShapeFunction<simCore::GOG::Cylinder>("start\n cylinder\n centerlla 24.4 43.2 0.0\n" + ELLIPTICAL_FIELDS + HEIGHT_FIELD + "end\n", testCylinderShapeOptionalFieldsFunc);
  // test ellipsoid (note that anglestart and angleend are ignored)
  rv += testShapeFunction<simCore::GOG::Ellipsoid>("start\n ellipsoid\n centerlla 24.4 43.2 0.0\n" + ELLIPTICAL_FIELDS + HEIGHT_FIELD + "end\n", testEllipsoidShapeOptionalFieldsFunc);
  // test cone
  rv += testShapeFunction<simCore::GOG::Cone>("start\n cone\n centerlla 24.4 43.2 0.0\n" + ELLIPTICAL_FIELDS + HEIGHT_FIELD + "end\n", testCircularHeightShapeOptionalFieldsFunc);

  // test arc with angleend
  rv += testShapeFunction<simCore::GOG::Arc>("start\n arc\n centerlla 24.4 43.2 0.0\n" + CIRCULAR_FIELDS + "angleStart 10.\n angleend 55.\n majoraxis 100.\n minoraxis 250.\n end\n", testEllipticalShapeOptionalFieldsFunc);
  // test arc with angleend, cannot cross 0
  rv += testShapeFunction<simCore::GOG::Arc>("start\n arc\n centerlla 24.4 43.2 0.0\n" + CIRCULAR_FIELDS + "angleStart 10.\n angleend -305.\n majoraxis 100.\n minoraxis 250.\n end\n", testEllipticalShapeOptionalFieldsFunc);

  return rv;
}

// test shapes that have required fields to ensure they are not created if required field is missing
int testIncompleteShapes()
{
  int rv = 0;
  simCore::GOG::Parser parser;
  std::vector<simCore::GOG::GogShapePtr> shapes;

  // test circle (requires center point)
  std::stringstream circleGogIncomplete;
  circleGogIncomplete << "start\n circle\n end\n";
  parser.parse(circleGogIncomplete, shapes);
  rv += SDK_ASSERT(shapes.size() == 0);
  shapes.clear();

  // test sphere (requires center point)
  std::stringstream sphereGogIncomplete;
  sphereGogIncomplete << "start\n sphere\n end\n";
  parser.parse(sphereGogIncomplete, shapes);
  rv += SDK_ASSERT(shapes.size() == 0);
  shapes.clear();

  // test hemisphere (requires center point)
  std::stringstream hemiGogIncomplete;
  hemiGogIncomplete << "start\n hemisphere\n end\n";
  parser.parse(hemiGogIncomplete, shapes);
  rv += SDK_ASSERT(shapes.size() == 0);
  shapes.clear();

  // test orbit (requires center point)
  std::stringstream orbitCtr2GogIncomplete;
  orbitCtr2GogIncomplete << "start\n orbit\n centerll2 23.4 45.2\n end\n";
  parser.parse(orbitCtr2GogIncomplete, shapes);
  rv += SDK_ASSERT(shapes.size() == 0);
  shapes.clear();

  // test orbit (requires center point 2)
  std::stringstream orbitCtrGogIncomplete;
  orbitCtrGogIncomplete << "start\n orbit\n centerll 23.4 45.2\n end\n";
  parser.parse(orbitCtrGogIncomplete, shapes);
  rv += SDK_ASSERT(shapes.size() == 0);
  shapes.clear();

  // test line (requires 2 points minimum)
  std::stringstream lineGogIncomplete;
  lineGogIncomplete << "start\n line\n lla 25.1 58.2 0.\n end\n";
  parser.parse(lineGogIncomplete, shapes);
  rv += SDK_ASSERT(shapes.size() == 0);
  shapes.clear();

  // test line segs (requires 2 points minimum)
  std::stringstream lineSegsGogIncomplete;
  lineSegsGogIncomplete << "start\n linesegs\n lla 25.1 58.2 0.\n end\n";
  parser.parse(lineSegsGogIncomplete, shapes);
  rv += SDK_ASSERT(shapes.size() == 0);
  shapes.clear();

  // test polygon (requires 3 points minimum)
  std::stringstream polyGogIncomplete;
  polyGogIncomplete << "start\n poly\n lla 25.1 58.2 0.\n lla 25.1 58.3 0.\n end\n";
  parser.parse(polyGogIncomplete, shapes);
  rv += SDK_ASSERT(shapes.size() == 0);
  shapes.clear();

  // test annotation (requires position)
  std::stringstream annoTextGog;
  annoTextGog << "start\n annotation label 1\n end\n";
  parser.parse(annoTextGog, shapes);
  rv += SDK_ASSERT(shapes.size() == 0);
  shapes.clear();

  // test annotation (requires text)
  std::stringstream annoCtrGog;
  annoCtrGog << "start\n annotation\n centerlla 24.2 43.3 0.\n end\n";
  parser.parse(annoCtrGog, shapes);
  rv += SDK_ASSERT(shapes.size() == 0);
  shapes.clear();

  // test arc (requires center point)
  std::stringstream arcGog;
  arcGog << "start\n arc\n end\n";
  parser.parse(arcGog, shapes);
  rv += SDK_ASSERT(shapes.size() == 0);
  shapes.clear();

  // test ellipse (requires center point)
  std::stringstream ellipseGog;
  ellipseGog << "start\n ellipse\n end\n";
  parser.parse(ellipseGog, shapes);
  rv += SDK_ASSERT(shapes.size() == 0);
  shapes.clear();

  // test cylinder (requires center point)
  std::stringstream cylinderGog;
  cylinderGog << "start\n cylinder\n end\n";
  parser.parse(cylinderGog, shapes);
  rv += SDK_ASSERT(shapes.size() == 0);
  shapes.clear();

  // test ellipsoid (requires center point)
  std::stringstream ellipsoidGog;
  ellipsoidGog << "start\n ellipsoid\n end\n";
  parser.parse(ellipsoidGog, shapes);
  rv += SDK_ASSERT(shapes.size() == 0);
  shapes.clear();

  // test cone (requires center point)
  std::stringstream coneGog;
  coneGog << "start\n cone\n end\n";
  parser.parse(coneGog, shapes);
  rv += SDK_ASSERT(shapes.size() == 0);
  shapes.clear();

  // test points (requires 1 point minimum)
  std::stringstream pointsGog;
  pointsGog << "start\n points\n end\n";
  parser.parse(pointsGog, shapes);
  rv += SDK_ASSERT(shapes.size() == 0);
  shapes.clear();

  return rv;
}

// test all the annotation fields and nested annotations special case
int testAnnotation()
{
  int rv = 0;
  simCore::GOG::Parser parser;
  std::vector<simCore::GOG::GogShapePtr> shapes;

  // test annotation with only required fields set
  std::stringstream annoMinimalGog;
  annoMinimalGog << "start\n annotation label 1\n centerll 24.5 54.6\n end\n";
  parser.parse(annoMinimalGog, shapes);
  rv += SDK_ASSERT(shapes.size() == 1);
  if (!shapes.empty())
  {
    simCore::GOG::Annotation* anno = dynamic_cast<simCore::GOG::Annotation*>(shapes.front().get());
    rv += SDK_ASSERT(anno != nullptr);
    if (anno)
    {
      rv += SDK_ASSERT(anno->text() == "label 1");
      rv += SDK_ASSERT(comparePositions(anno->position(), simCore::Vec3(24.5 * simCore::DEG2RAD, 54.6 * simCore::DEG2RAD, 0.)));
      // make sure optional fields were not set
      std::string fontName;
      rv += SDK_ASSERT(anno->getFontName(fontName) != 0);
      int textSize = 0;
      rv += SDK_ASSERT(anno->getTextSize(textSize) != 0);
      rv += SDK_ASSERT(textSize == 15);
      simCore::GOG::GogShape::Color textColor(0,255,255,0);
      rv += SDK_ASSERT(anno->getTextColor(textColor) != 0);
      rv += SDK_ASSERT(textColor == simCore::GOG::GogShape::Color());
      simCore::GOG::GogShape::Color outlineColor(0,255,255,0);
      rv += SDK_ASSERT(anno->getOutlineColor(outlineColor) != 0);
      rv += SDK_ASSERT(outlineColor == simCore::GOG::GogShape::Color());
      simCore::GOG::Annotation::OutlineThickness thickness = simCore::GOG::Annotation::OutlineThickness::THICK;
      rv += SDK_ASSERT(anno->getOutlineThickness(thickness) != 0);
      rv += SDK_ASSERT(thickness == simCore::GOG::Annotation::OutlineThickness::NONE);
      std::string iconFile = "someFile";
      rv += SDK_ASSERT(anno->getIconFile(iconFile) != 0);
      rv += SDK_ASSERT(iconFile.empty());
    }
  }
  shapes.clear();

  // test full annotation
  std::stringstream annoGog;
  annoGog << "start\n annotation label 1\n centerll 24.5 54.6\n fontname georgia.ttf\n fontsize 24\n linecolor hex 0xa0ffa0ff\n textoutlinethickness thin\n textoutlinecolor blue\n# kml_icon icon.png\n end\n";
  parser.parse(annoGog, shapes);
  rv += SDK_ASSERT(shapes.size() == 1);
  if (!shapes.empty())
  {
    simCore::GOG::Annotation* anno = dynamic_cast<simCore::GOG::Annotation*>(shapes.front().get());
    rv += SDK_ASSERT(anno != nullptr);
    if (anno)
    {
      rv += SDK_ASSERT(comparePositions(anno->position(), simCore::Vec3(24.5 * simCore::DEG2RAD, 54.6 * simCore::DEG2RAD, 0.)));
      rv += SDK_ASSERT(anno->text() == "label 1");
      std::string fontName;
      rv += SDK_ASSERT(anno->getFontName(fontName) == 0);
      rv += SDK_ASSERT(fontName.find("georgia.ttf") != std::string::npos);
      int textSize = 0;
      rv += SDK_ASSERT(anno->getTextSize(textSize) == 0);
      rv += SDK_ASSERT(textSize == 24);
      simCore::GOG::GogShape::Color textColor;
      rv += SDK_ASSERT(anno->getTextColor(textColor) == 0);
      rv += SDK_ASSERT(textColor == simCore::GOG::GogShape::Color(255, 160, 255, 160));
      simCore::GOG::GogShape::Color outlineColor;
      rv += SDK_ASSERT(anno->getOutlineColor(outlineColor) == 0);
      rv += SDK_ASSERT(outlineColor == simCore::GOG::GogShape::Color(0, 0, 255, 255));
      simCore::GOG::Annotation::OutlineThickness thickness = simCore::GOG::Annotation::OutlineThickness::NONE;
      rv += SDK_ASSERT(anno->getOutlineThickness(thickness) == 0);
      rv += SDK_ASSERT(thickness == simCore::GOG::Annotation::OutlineThickness::THIN);
      std::string iconFile;
      rv += SDK_ASSERT(anno->getIconFile(iconFile) == 0);
      rv += SDK_ASSERT(iconFile == "icon.png");
    }
  }
  shapes.clear();

  // test nested annotations
  std::stringstream annoNestedGog;
  annoNestedGog << "start\n annotation label 0\n centerll 24.5 54.6\n fontname georgia.ttf\n fontsize 24\n linecolor hex 0xa0ffa0ff\n textoutlinethickness thin\n textoutlinecolor blue\n"
    << "annotation label 1\n centerll 24.7 54.3\n annotation label 2\n centerll 23.4 55.4\n end\n";
  parser.parse(annoNestedGog, shapes);
  rv += SDK_ASSERT(shapes.size() == 3);
  if (!shapes.empty())
  {
    int textId = 0;
    std::vector<simCore::Vec3> positions;
    positions.push_back(simCore::Vec3(24.5 * simCore::DEG2RAD, 54.6 * simCore::DEG2RAD, 0.));
    positions.push_back(simCore::Vec3(24.7 * simCore::DEG2RAD, 54.3 * simCore::DEG2RAD, 0.));
    positions.push_back(simCore::Vec3(23.4* simCore::DEG2RAD, 55.4 * simCore::DEG2RAD, 0.));

    // check that all 3 annotations have the same attributes, since they should all match the first annotation fields found
    for (simCore::GOG::GogShapePtr gogPtr : shapes)
    {
      simCore::GOG::Annotation* anno = dynamic_cast<simCore::GOG::Annotation*>(gogPtr.get());
      rv += SDK_ASSERT(anno != nullptr);
      if (anno)
      {
        rv += SDK_ASSERT(comparePositions(anno->position(), positions[textId]));
        std::ostringstream os;
        os << "label " << textId++;
        rv += SDK_ASSERT(anno->text() == os.str());

        std::string fontName;
        rv += SDK_ASSERT(anno->getFontName(fontName) == 0);
        rv += SDK_ASSERT(fontName.find("georgia.ttf") != std::string::npos);
        int textSize = 0;
        rv += SDK_ASSERT(anno->getTextSize(textSize) == 0);
        rv += SDK_ASSERT(textSize == 24);
        simCore::GOG::GogShape::Color textColor;
        rv += SDK_ASSERT(anno->getTextColor(textColor) == 0);
        rv += SDK_ASSERT(textColor == simCore::GOG::GogShape::Color(255, 160, 255, 160));
        simCore::GOG::GogShape::Color outlineColor;
        rv += SDK_ASSERT(anno->getOutlineColor(outlineColor) == 0);
        rv += SDK_ASSERT(outlineColor == simCore::GOG::GogShape::Color(0, 0, 255, 255));
        simCore::GOG::Annotation::OutlineThickness thickness = simCore::GOG::Annotation::OutlineThickness::NONE;
        rv += SDK_ASSERT(anno->getOutlineThickness(thickness) == 0);
        rv += SDK_ASSERT(thickness == simCore::GOG::Annotation::OutlineThickness::THIN);
      }
    }
  }
  shapes.clear();

  return rv;
}

int testUnits()
{
  int rv = 0;
  simCore::GOG::Parser parser;
  std::vector<simCore::GOG::GogShapePtr> shapes;

  // test circle range units default to yards and altitude units default to feet
  std::stringstream circleGog;
  circleGog << "start\n circle\n centerlla 25.1 58.2 12.\n radius 100\n end\n";
  parser.parse(circleGog, shapes);
  rv += SDK_ASSERT(shapes.size() == 1);
  if (!shapes.empty())
  {
    simCore::GOG::Circle* circle = dynamic_cast<simCore::GOG::Circle*>(shapes.front().get());
    rv += SDK_ASSERT(circle != nullptr);
    if (circle)
    {
      const simCore::Vec3& center = circle->centerPosition();
      simCore::Units altMeters(simCore::Units::METERS);
      simCore::Units altFeet(simCore::Units::FEET);
      // verify output in meters matches input in feet
      rv += SDK_ASSERT(comparePositions(center, simCore::Vec3(25.1 * simCore::DEG2RAD, 58.2 * simCore::DEG2RAD, altFeet.convertTo(altMeters, 12.))));

      double radius = 0.;
      // verify output in meters matches input in feet
      rv += SDK_ASSERT(circle->getRadius(radius) == 0);
      simCore::Units altYards(simCore::Units::YARDS);
      rv += SDK_ASSERT(simCore::areEqual(radius, altYards.convertTo(altMeters, 100.)));
    }
  }
  shapes.clear();

  // test circle with defined range and altitude units
  std::stringstream circleDefinedGog;
  circleDefinedGog << "start\n circle\n centerlla 25.1 58.2 10.\n radius 10\n rangeunits km\n altitudeunits m\n end\n";
  parser.parse(circleDefinedGog, shapes);
  rv += SDK_ASSERT(shapes.size() == 1);
  if (!shapes.empty())
  {
    simCore::GOG::Circle* circle = dynamic_cast<simCore::GOG::Circle*>(shapes.front().get());
    rv += SDK_ASSERT(circle != nullptr);
    if (circle)
    {
      const simCore::Vec3& center = circle->centerPosition();
      // verify output in meters matches input in meters
      rv += SDK_ASSERT(comparePositions(center, simCore::Vec3(25.1 * simCore::DEG2RAD, 58.2 * simCore::DEG2RAD, 10.)));

      double radius = 0.;
      // verify radius is 10 km
      rv += SDK_ASSERT(circle->getRadius(radius) == 0);
      rv += SDK_ASSERT(simCore::areEqual(radius, 10000.));
    }
  }
  shapes.clear();

  // test line altitude units default to feet
  std::stringstream lineGog;
  lineGog << "start\n line\n lla 25.1 58.2 20.\n lla 26.2 58.3 12.\n end\n";
  parser.parse(lineGog, shapes);
  rv += SDK_ASSERT(shapes.size() == 1);
  if (!shapes.empty())
  {
    simCore::GOG::Line* line = dynamic_cast<simCore::GOG::Line*>(shapes.front().get());
    rv += SDK_ASSERT(line != nullptr);
    if (line)
    {
      const std::vector<simCore::Vec3>& positions = line->points();
      rv += SDK_ASSERT(positions.size() == 2);
      std::vector<simCore::Vec3> input;
      simCore::Units altMeters(simCore::Units::METERS);
      simCore::Units altFeet(simCore::Units::FEET);
      input.push_back(simCore::Vec3(25.1 * simCore::DEG2RAD, 58.2 * simCore::DEG2RAD, altFeet.convertTo(altMeters, 20.)));
      input.push_back(simCore::Vec3(26.2 * simCore::DEG2RAD, 58.3 * simCore::DEG2RAD, altFeet.convertTo(altMeters, 12.)));
      rv += SDK_ASSERT(comparePositionVectors(input, positions));
    }
  }
  shapes.clear();

  // test line with defined altitude units
  std::stringstream lineDefinedGog;
  lineDefinedGog << "start\n line\n lla 25.1 58.2 1.4\n lla 26.2 58.3 2.\n altitudeunits kf\n end\n";
  parser.parse(lineDefinedGog, shapes);
  rv += SDK_ASSERT(shapes.size() == 1);
  if (!shapes.empty())
  {
    simCore::GOG::Line* line = dynamic_cast<simCore::GOG::Line*>(shapes.front().get());
    rv += SDK_ASSERT(line != nullptr);
    if (line)
    {
      const std::vector<simCore::Vec3>& positions = line->points();
      rv += SDK_ASSERT(positions.size() == 2);
      std::vector<simCore::Vec3> input;
      simCore::Units altMeters(simCore::Units::METERS);
      simCore::Units altKf(simCore::Units::KILOFEET);
      input.push_back(simCore::Vec3(25.1 * simCore::DEG2RAD, 58.2 * simCore::DEG2RAD, altKf.convertTo(altMeters, 1.4)));
      input.push_back(simCore::Vec3(26.2 * simCore::DEG2RAD, 58.3 * simCore::DEG2RAD, altKf.convertTo(altMeters, 2.)));
      rv += SDK_ASSERT(comparePositionVectors(input, positions));
    }
  }
  shapes.clear();

  // test arc angle units default to degrees
  std::stringstream arcGog;
  arcGog << "start\n arc\n centerlla 25.1 58.2 12.\n anglestart 5.\n angledeg 100.\n end\n";
  parser.parse(arcGog, shapes);
  rv += SDK_ASSERT(shapes.size() == 1);
  if (!shapes.empty())
  {
    simCore::GOG::Arc* arc = dynamic_cast<simCore::GOG::Arc*>(shapes.front().get());
    rv += SDK_ASSERT(arc != nullptr);
    if (arc)
    {
      double angleStart = 0.;
      arc->getAngleStart(angleStart);
      // verify output in radians matches input in degrees
      rv += SDK_ASSERT(simCore::areEqual(angleStart * simCore::RAD2DEG, 5.));

      double angleSweep = 0.;
      arc->getAngleSweep(angleSweep);
      rv += SDK_ASSERT(simCore::areEqual(angleSweep * simCore::RAD2DEG, 100.));
    }
  }
  shapes.clear();

  // test arc with defined angle units
  std::stringstream arcDefinedGog;
  arcDefinedGog << "start\n arc\n centerlla 25.1 58.2 12.\n anglestart 0.1253\n angledeg 1.5\n angleunits rad\n end\n";
  parser.parse(arcDefinedGog, shapes);
  rv += SDK_ASSERT(shapes.size() == 1);
  if (!shapes.empty())
  {
    simCore::GOG::Arc* arc = dynamic_cast<simCore::GOG::Arc*>(shapes.front().get());
    rv += SDK_ASSERT(arc != nullptr);
    if (arc)
    {
      double angleStart = 0.;
      arc->getAngleStart(angleStart);
      rv += SDK_ASSERT(simCore::areEqual(angleStart, 0.1253));

      double angleSweep = 0.;
      arc->getAngleSweep(angleSweep);
      rv += SDK_ASSERT(simCore::areEqual(angleSweep, 1.5));
    }
  }
  shapes.clear();

  return rv;
}

}

int GogTest(int argc, char* argv[])
{
  simCore::checkVersionThrow();
  int rv = 0;

  rv += testGeneralSyntax();
  rv += testMinimalShapes();
  rv += testIncompleteShapes();
  rv += testShapesOptionalFields();
  rv += testAnnotation();
  rv += testUnits();

  return rv;
}

