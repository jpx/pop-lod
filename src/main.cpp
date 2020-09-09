#include "minko/Minko.hpp"
#include "minko/MinkoSDL.hpp"
#include "minko/MinkoSerializer.hpp"
#include "minko/MinkoStreaming.hpp"

using namespace minko;
using namespace minko::component;
using namespace minko::data;
using namespace minko::extension;
using namespace minko::file;
using namespace minko::geometry;
using namespace minko::material;
using namespace minko::scene;

// Relative path of the .scene to be streamed in.
constexpr auto MODEL_FILENAME       = "xyzrgb_dragon.ply.withcracks.scene";
// Blending time between two consecutive LODs.
constexpr auto LOD_BLENDING_PERIOD  = 1500.0f;

int
main(int argc, char** argv)
{
    // Setup canvas and file loaders.
    const auto canvas = Canvas::create("POP LOD Demo");
    const auto sceneManager = SceneManager::create(canvas);
    const auto defaultLoader = sceneManager->assets()->loader();
    const auto fxLoader = file::Loader::create(defaultLoader);

    // Setup geometry streaming.
    const auto lodScheduler = MasterLodScheduler::create();
    SerializerExtension::activateExtension<StreamingExtension>()
        ->initialize(lodScheduler->streamingOptions()
            ->popGeometryErrorToleranceThreshold(0)                 // Set sub-pixel error tolerance.
            ->popGeometryLodBlendingEnabled(true)                   // Enable LOD blending.
            ->popGeometryLodBlendingPeriod(LOD_BLENDING_PERIOD)     // Set the LOD blending period.
            ->popGeometryLodRangeFetchingBoundFunction([](
                int currentLod,
                int requiredLod,
                int& lodRangeMinSize,
                int& lodRangeMaxSize,
                int& lodRangeRequestMinSize,
                int& lodRangeRequestMaxSize) {
                    // Load a single LOD at a time.
                    return 1;
                }
            )
            ->maxNumActiveParsers(8)                                // Limit the number of parallel jobs.
        );

    // Setup loading of the model to be executed
    // after loading the effect.
    const auto fxLoaderComplete = fxLoader->complete()->connect([&defaultLoader](file::Loader::Ptr loader)
    {
        const auto effect = loader->options()->assetLibrary()->effect("effect/PopLod.effect");

        defaultLoader->options()
            ->registerParser<file::SceneParser>("scene")
            ->effect(effect)
            ->nodeFunction([effect](Node::Ptr n)
            {
                // Override the effect of each surface
                // defined in the .scene with the "PopLod.effect".
                if (n->hasComponent<Surface>())
                {
                    n->component<Surface>()->effect(effect);
                }
                return n;
            });

        // Load the minimum amount of data from the .scene
        // as the rest will progressively and
        // automatically be streamed in.
        defaultLoader
            ->queue(MODEL_FILENAME, defaultLoader->options()->clone()
                ->seekedLength(MINKO_SCENE_HEADER_SIZE)
            )
            ->load();
    });

    // Create the scene root node.
    const auto root = scene::Node::create("root")
        ->addComponent(sceneManager)
        ->addComponent(lodScheduler);

    root->data().addProvider(Provider::create()->set({
        { "popLodBlendingPeriod", LOD_BLENDING_PERIOD }
    }));

    // Setup camera.
    static constexpr auto Z_NEAR = 0.1f;
    static constexpr auto Z_FAR = 1000.0f;
    static constexpr auto FOV = 0.785f;

    const auto camera = scene::Node::create("camera")
        ->addComponent(Renderer::create(0x7f7f7fff))
        ->addComponent(Transform::create())
        ->addComponent(Camera::create(math::perspective(FOV, canvas->aspectRatio(), Z_NEAR, Z_FAR)));

    // Expose camera parameters to the scene,
    // as required by the geometry streaming components.
    camera->component<Camera>()->data()->set({
        { "zNear", Z_NEAR },
        { "zFar", Z_FAR },
        { "aspectRatio", canvas->aspectRatio() },
        { "fov", FOV }
    });

    root->addChild(camera);

    const auto defaultLoaderComplete = defaultLoader->complete()->connect([=](file::Loader::Ptr loader)
    {
        auto sceneNode = sceneManager->assets()->symbol(MODEL_FILENAME);

        root->addChild(sceneNode);

        if (!sceneNode->hasComponent<Transform>())
            sceneNode->addComponent(Transform::create());

        // Setup lighting.
        root
            ->addChild(Node::create("dirLight")
                ->addComponent(DirectionalLight::create())
                ->addComponent(Transform::create(math::inverse(
                    math::lookAt(math::vec3(5.0f, 20.0f, 0.0f), math::vec3(0.0f), math::vec3(0.0f, 1.0f, 0.0f))
            ))))
            ->addChild(Node::create("headSpot")
                ->addComponent(SpotLight::create(0.3f, 0.3f))
                ->addComponent(Transform::create(math::inverse(
                    math::lookAt(math::vec3(100.0f, 50.0f, 0.0f), math::vec3(75.0f, 0.0f, 0.0f), math::vec3(0.0f, 1.0f, 0.0f))
            ))))
            ->addChild(Node::create("headPoint")
                ->addComponent(PointLight::create(0.25f, 0.25f)->color(math::vec3(1.0f, 0.0f, 0.0f)))
                ->addComponent(Transform::create(math::translate(math::vec3(70.0f, -40.0f, 0.0f)))))
            ->addChild(Node::create("tailPoint")
                ->addComponent(PointLight::create(0.25f, 0.25f)->color(math::vec3(1.0f, 0.0f, 0.0f)))
                ->addComponent(Transform::create(math::translate(math::vec3(-70.0f, -40.0f, 0.0f)))))
            ->addChild(Node::create("midPoint")
                ->addComponent(PointLight::create(0.5f, 0.5f)->color(math::vec3(1.0f, 0.0f, 0.0f)))
                ->addComponent(Transform::create(math::translate(math::vec3(0.0f, -40.0f, 0.0f)))))
            ->addChild(Node::create("ambientLight")
                ->addComponent(AmbientLight::create()));

            // Setup ground mesh.
            root->addChild(Node::create()
                ->addComponent(Transform::create(
                    math::translate(math::vec3(0.0f, -40.0f, 0.0f)) *
                    math::scale(math::vec3(300.0f)) *
                    math::rotate(-math::half_pi<float>(), math::vec3(1.0f, 0.0f, 0.0f))
                ))
                ->addComponent(Surface::create(
                    QuadGeometry::create(defaultLoader->options()->context()),
                    PhongMaterial::create()->diffuseColor(math::vec4(0.5f)),
                    defaultLoader->options()->effect()
                ))
            );
    });

    const auto canvasResized = canvas->resized()->connect([camera](AbstractCanvas::Ptr canvas, unsigned int w, unsigned int h)
    {
        camera->component<Camera>()->projectionMatrix(math::perspective(FOV, float(w) / float(h), Z_NEAR, Z_FAR));
    });

    // Setup camera control.
    const auto minPitch = math::epsilon<float>();
    const auto maxPitch = math::half_pi<float>() - math::epsilon<float>();
    const auto lookAtTarget = math::vec3(0.0f, 0.0f, 0.0f);
    auto yaw = -math::half_pi<float>() + 0.25f;
    auto pitch = math::pi<float>() * 0.36f;
    auto roll = 0.0f;
    auto distance = 250.0f;
    auto cameraRotationXSpeed = 0.0f;
    auto cameraRotationYSpeed = 0.0f;

    const auto mouseWheel = canvas->mouse()->wheel()->connect([&](input::Mouse::Ptr m, int h, int v)
    {
        distance += float(v) * 10;
    });

    const auto mouseMove = canvas->mouse()->move()->connect([&cameraRotationXSpeed, &cameraRotationYSpeed](input::Mouse::Ptr m, int dx, int dy)
    {
        if (m->leftButtonIsDown())
        {
            cameraRotationYSpeed = float(dx) * 0.01f;
            cameraRotationXSpeed = float(dy) * -0.01f;
        }
    });

    const auto canvasEnterFrame = canvas->enterFrame()->connect([&](AbstractCanvas::Ptr c, float time, float deltaTime, bool shouldRender)
    {
        // Update the camera.
        yaw += cameraRotationYSpeed;
        cameraRotationYSpeed *= 0.9f;
        pitch += cameraRotationXSpeed;
        cameraRotationXSpeed *= 0.9f;

        if (pitch > maxPitch)
            pitch = maxPitch;
        else if (pitch < minPitch)
            pitch = minPitch;

        camera->component<Transform>()->matrix(math::inverse(math::lookAt((
            math::vec3(
                lookAtTarget.x + distance * std::cos(yaw) * std::sin(pitch),
                lookAtTarget.y + distance * std::cos(pitch),
                lookAtTarget.z + distance * std::sin(yaw) * std::sin(pitch)
            )),
            lookAtTarget,
            math::vec3(0.0f, 1.0f, 0.0f)
        )));

        // Advance the scene simulation by a single tick.
        sceneManager->nextFrame(time, deltaTime, shouldRender);
    });

    // Load the effect to start the loading chain.
    fxLoader
        ->queue("effect/PopLod.effect")
        ->load();

    // Run the main loop.
    canvas->run();
}