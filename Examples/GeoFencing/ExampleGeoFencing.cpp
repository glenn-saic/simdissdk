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
 * License for source code is in accompanying LICENSE.txt file. If you did
 * not receive a LICENSE.txt with this code, email simdis@nrl.navy.mil.
 *
 * The U.S. Government retains all rights to use, duplicate, distribute,
 * disclose, or release this software.
 *
 */

/**
 * Demonstrates the use of the simCore::GeoFence to monitor a geospatial region.
 */

#include "simCore/Common/Version.h"
#include "simCore/Common/HighPerformanceGraphics.h"
#include "simCore/Calc/Angle.h"
#include "simCore/Calc/Geometry.h"
#include "simVis/Constants.h"
#include "simVis/Scenario.h"
#include "simVis/SceneManager.h"
#include "simVis/Viewer.h"
#include "simUtil/ExampleResources.h"

#include "osgEarth/Geometry"
#include "osgEarth/Feature"
#include "osgEarth/FeatureNode"

#ifdef HAVE_IMGUI
#include "SimExamplesGui.h"
#include "OsgImGuiHandler.h"
#else
#include "osgEarth/Controls"
namespace ui = osgEarth::Util::Controls;
#endif

//----------------------------------------------------------------------------

// Application data for the demo.
struct AppData
{
  std::vector<simCore::GeoFence> fences;
#ifdef HAVE_IMGUI
  std::string feedbackText;
#else
  osg::ref_ptr<ui::LabelControl> feedbackLabel;
#endif
  osg::ref_ptr<osgEarth::MapNode> mapnode;

  void setFeedbackText(const std::string& text)
  {
#ifdef HAVE_IMGUI
    feedbackText = text;
#else
    feedbackLabel->setText(text);
#endif
  }
};

//----------------------------------------------------------------------------

#ifdef HAVE_IMGUI

struct ControlPanel : public simExamples::SimExamplesGui
{
  explicit ControlPanel(AppData& app)
    : simExamples::SimExamplesGui("GeoFencing Test Example"),
    app_(app)
  {
  }

  void draw(osg::RenderInfo& ri) override
  {
    if (!isVisible())
      return;

    if (firstDraw_)
    {
      ImGui::SetNextWindowPos(ImVec2(5, 25));
      firstDraw_ = false;
    }
    ImGui::SetNextWindowBgAlpha(.6f);
    ImGui::Begin(name(), visible(), ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), "The yellow areas are geofences.");
    ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "The red areas are invalid (concave) geofences.");
    ImGui::Text("Click to see whether you are inside one!");
    if (!app_.feedbackText.empty())
      ImGui::Text(app_.feedbackText.c_str());

    ImGui::End();
  }

private:
  AppData& app_;
};

#else
namespace
{
  ui::Control* createUI(AppData* app)
  {
    // vbox returned to caller as caller's memory
    ui::VBox* vbox = new ui::VBox();
    vbox->setAbsorbEvents(true);
    vbox->setVertAlign(ui::Control::ALIGN_TOP);
    vbox->setPadding(10);
    vbox->setBackColor(0.f, 0.f, 0.f, 0.4f);
    vbox->addControl(new ui::LabelControl("GeoFencing Test", 20.f));
    vbox->addControl(new ui::LabelControl("The yellow areas are geofences.", simVis::Color::Yellow));
    vbox->addControl(new ui::LabelControl("The red areas are invalid (concave) geofences.", simVis::Color::Red));
    vbox->addControl(new ui::LabelControl("Click to see whether you are inside one!"));
    app->feedbackLabel = vbox->addControl(new ui::LabelControl());

    return vbox;
  }
}
#endif
//----------------------------------------------------------------------------

namespace
{
  /// multiplies a vec3 by a scalar value
  simCore::Vec3 operator * (const simCore::Vec3& i, double scalar)
  {
    return simCore::Vec3(i.x()*scalar, i.y()*scalar, i.z()*scalar);
  }

  /// styles a feature
  void styleAnnotation(osgEarth::Style& style, bool valid)
  {
    namespace sym = osgEarth;
    const simVis::Color color = valid ? simVis::Color::Yellow : simVis::Color::Red;
    style.getOrCreate<sym::PolygonSymbol>()->fill()->color() = simVis::Color(color, 0.5f);
    style.getOrCreate<sym::LineSymbol>()->stroke()->color() = simVis::Color::White;
    style.getOrCreate<sym::LineSymbol>()->stroke()->width() = 2.f;
    style.getOrCreate<sym::LineSymbol>()->tessellationSize()->set(100, osgEarth::Units::KILOMETERS);
    style.getOrCreate<sym::AltitudeSymbol>()->verticalOffset() = 10000;
    style.getOrCreate<sym::RenderSymbol>()->backfaceCulling() = false;

    // Turn off depth testing, and enable the horizon clip plane (SDK-43)
    style.getOrCreate<sym::RenderSymbol>()->depthTest() = false;
    style.getOrCreate<sym::RenderSymbol>()->clipPlane() = simVis::CLIPPLANE_VISIBLE_HORIZON;
  }

  /// draws a fence on the map
  osg::Node* buildFenceAnnotation(const simCore::Vec3String& v, bool valid, osgEarth::MapNode* mapnode)
  {
    // convert it to an osgEarth geometry:
    osg::ref_ptr<osgEarth::Polygon> geom = new osgEarth::Polygon();
    for (unsigned i = 0; i < v.size(); ++i)
    {
      const simCore::Vec3 deg = v[i] * simCore::RAD2DEG;
      geom->push_back(osg::Vec3d(deg.y(), deg.x(), deg.z()));
    }
    geom->open();

    // make and style a feature:
    osg::ref_ptr<osgEarth::Feature> feature = new osgEarth::Feature(geom.get(), mapnode->getMap()->getSRS());
    styleAnnotation(feature->style().mutable_value(), valid);
    feature->geoInterp() = osgEarth::GEOINTERP_GREAT_CIRCLE;

    osgEarth::FeatureNode* featureNode = new osgEarth::FeatureNode(feature.get());
    featureNode->setMapNode(mapnode);
    return featureNode;
  }

  /// creates all the fences
  void buildFences(AppData& app, simVis::SceneManager* scene)
  {
    // fence 1 : a simple poly that doesn't overlap anything.
    {
      simCore::Vec3String p;
      p.push_back(simCore::Vec3(34, -121, 0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(32,  -93, 0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(47,  -94, 0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(45, -122, 0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(34, -121, 0) * simCore::DEG2RAD);
      simCore::GeoFence fence(p, simCore::COORD_SYS_LLA);
      app.fences.push_back(fence);
      scene->getScenario()->addChild(buildFenceAnnotation(p, fence.valid(), scene->getMapNode()));
    }

    // fence 2 : a fence spanning the north pole!
    {
      simCore::Vec3String p;
      p.push_back(simCore::Vec3(60,    0, 0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(60,   60, 0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(60,  140, 0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(75, -140, 0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(60,    0, 0) * simCore::DEG2RAD);
      simCore::GeoFence fence(p, simCore::COORD_SYS_LLA);
      app.fences.push_back(fence);
      scene->getScenario()->addChild(buildFenceAnnotation(p, fence.valid(), scene->getMapNode()));
    }

    // fence 3 : a fence spanning the south pole!
    {
      simCore::Vec3String p;
      p.push_back(simCore::Vec3(-50, -120, 0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(-50, -140, 0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(-50,   40, 0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(-50,    0, 0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(-50, -120, 0) * simCore::DEG2RAD);
      simCore::GeoFence fence(p, simCore::COORD_SYS_LLA);
      app.fences.push_back(fence);
      scene->getScenario()->addChild(buildFenceAnnotation(p, fence.valid(), scene->getMapNode()));
    }

    // fence 4 : a fence spanning the anti-meridian!
    {
      simCore::Vec3String p;
      p.push_back(simCore::Vec3(20,  140, 0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(-20,  140, 0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(-20, -140, 0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(20, -140, 0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(20,  140, 0) * simCore::DEG2RAD);
      simCore::GeoFence fence(p, simCore::COORD_SYS_LLA);
      app.fences.push_back(fence);
      scene->getScenario()->addChild(buildFenceAnnotation(p, fence.valid(), scene->getMapNode()));
    }

    // fence 5 : an invalid geofence (because it's not convex)
    {
      simCore::Vec3String p;
      p.push_back(simCore::Vec3(0,   0,  0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(0,  30,  0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(30,  30,  0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(15,  15,  0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(30,   0,  0) * simCore::DEG2RAD);
      p.push_back(simCore::Vec3(0,   0,  0) * simCore::DEG2RAD);
      simCore::GeoFence fence(p, simCore::COORD_SYS_LLA);
      app.fences.push_back(fence);
      scene->getScenario()->addChild(buildFenceAnnotation(p, fence.valid(), scene->getMapNode()));
    }
  }

  /// event handler to test whether mouse clicks are inside a fence.
  struct Tester : public osgGA::GUIEventHandler
  {
    AppData* app_;
    explicit Tester(AppData* app) : app_(app) { }
    bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa, osg::Object*, osg::NodeVisitor*)
    {
      if (ea.getEventType() == ea.PUSH)
      {
        osg::Vec3d world;
        if (app_->mapnode->getTerrain()->getWorldCoordsUnderMouse(aa.asView(), ea.getX(), ea.getY(), world))
        {
          simCore::Vec3 ecef(world.x(), world.y(), world.z());

          for (unsigned i = 0; i < app_->fences.size(); ++i)
          {
            // This is how to test an ECEF simCore::Vec3 point against a GeoFence.
            // You could also pass in a simCore::Coordinate.
            const simCore::GeoFence& fence = app_->fences[i];

            if (fence.valid() && fence.contains(ecef))
            {
              app_->setFeedbackText("Inside a fence!");
              return true;
            }
          }
          app_->setFeedbackText("No.");
        }
        else
        {
          app_->setFeedbackText("You clicked off the terrain.");
        }
      }
      return false;
    }
  };
}

//----------------------------------------------------------------------------

int main(int argc, char **argv)
{
  // Set up the scene:
  simCore::checkVersionThrow();
  simExamples::configureSearchPaths();
  osg::ref_ptr<osgEarth::Map> map = simExamples::createDefaultExampleMap();

  osg::ref_ptr<simVis::Viewer> viewer = new simVis::Viewer();
  viewer->setMap(map.get());
  viewer->setNavigationMode(simVis::NAVMODE_ROTATEPAN);

  // add sky node
  simExamples::addDefaultSkyNode(viewer.get());

  // Application data:
  AppData app;
  app.mapnode = viewer->getSceneManager()->getMapNode();

  // Generate some fences.
  buildFences(app, viewer->getSceneManager());

#ifdef HAVE_IMGUI
  // Pass in existing realize operation as parent op, parent op will be called first
  viewer->getViewer()->setRealizeOperation(new GUI::OsgImGuiHandler::RealizeOperation(viewer->getViewer()->getRealizeOperation()));
  GUI::OsgImGuiHandler* gui = new GUI::OsgImGuiHandler();
  viewer->getMainView()->getEventHandlers().push_front(gui);
  gui->add(new ControlPanel(app));
#else
  // Install the UI:
  viewer->getMainView()->addOverlayControl(createUI(&app));
#endif

  // Install the click handler:
  viewer->addEventHandler(new Tester(&app));

  // add some stock OSG handlers and go
  viewer->installDebugHandlers();
  return viewer->run();
}
