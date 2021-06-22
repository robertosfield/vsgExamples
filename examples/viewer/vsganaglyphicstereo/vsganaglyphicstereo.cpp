#include <vsg/all.h>

#include <chrono>
#include <iostream>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

namespace vsg
{
    class VSG_DECLSPEC PerViewGraphicsPipelineState : public Inherit<GraphicsPipelineState, PerViewGraphicsPipelineState>
    {
    public:
        PerViewGraphicsPipelineState() {}

        std::vector<ref_ptr<GraphicsPipelineState>> graphicsPipesStates;

        virtual void apply(Context& context, VkGraphicsPipelineCreateInfo& pipelineInfo) const
        {
            auto viewID = context.viewID;
            if (viewID < graphicsPipesStates.size()) graphicsPipesStates[viewID]->apply(context, pipelineInfo);
        }

    protected:
        virtual ~PerViewGraphicsPipelineState() {}
    };
    VSG_type_name(vsg::PerViewGraphicsPipelineState);
}

class ReplaceColorBlendState : public vsg::Visitor
{
public:

    std::set<vsg::GraphicsPipeline*> visited;

    void apply(vsg::Node& node) override
    {
        node.traverse(*this);
    }

    void apply(vsg::StateGroup& sg) override
    {
        for(auto& sg : sg.stateCommands)
        {
            sg->accept(*this);
        }
    }

    void apply(vsg::BindGraphicsPipeline& bgp) override
    {
        auto gp = bgp.pipeline;
        if (visited.count(gp.get()) > 0)
        {
            return;
        }

        visited.insert(gp);

        for(auto itr = gp->pipelineStates.begin(); itr != gp->pipelineStates.end(); ++itr)
        {
            if ((*itr)->is_compatible(typeid(vsg::ColorBlendState)))
            {
                auto colorBlendState = (*itr)->cast<vsg::ColorBlendState>();
                auto perViewGraphicsPipelineState = vsg::PerViewGraphicsPipelineState::create();

                VkPipelineColorBlendAttachmentState left_colorBlendAttachment = {
                    VK_FALSE,                                                                                                 // blendEnable
                    VK_BLEND_FACTOR_ZERO,                                                                                     // srcColorBlendFactor
                    VK_BLEND_FACTOR_ZERO,                                                                                     // dstColorBlendFactor
                    VK_BLEND_OP_ADD,                                                                                          // colorBlendOp
                    VK_BLEND_FACTOR_ZERO,                                                                                     // srcAlphaBlendFactor
                    VK_BLEND_FACTOR_ZERO,                                                                                     // dstAlphaBlendFactor
                    VK_BLEND_OP_ADD,                                                                                          // alphaBlendOp
                    VK_COLOR_COMPONENT_R_BIT /*| VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT*/ | VK_COLOR_COMPONENT_A_BIT // colorWriteMask
                };

                VkPipelineColorBlendAttachmentState right_colorBlendAttachment = {
                    VK_FALSE,                                                                                                 // blendEnable
                    VK_BLEND_FACTOR_ZERO,                                                                                     // srcColorBlendFactor
                    VK_BLEND_FACTOR_ZERO,                                                                                     // dstColorBlendFactor
                    VK_BLEND_OP_ADD,                                                                                          // colorBlendOp
                    VK_BLEND_FACTOR_ZERO,                                                                                     // srcAlphaBlendFactor
                    VK_BLEND_FACTOR_ZERO,                                                                                     // dstAlphaBlendFactor
                    VK_BLEND_OP_ADD,                                                                                          // alphaBlendOp
                    /*VK_COLOR_COMPONENT_R_BIT | */VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT // colorWriteMask
                };

                auto left_colorBlendState = vsg::ColorBlendState::create(*colorBlendState);
                left_colorBlendState->attachments = {left_colorBlendAttachment};

                auto right_colorBlendState = vsg::ColorBlendState::create(*colorBlendState);
                right_colorBlendState->attachments = {right_colorBlendAttachment};

                perViewGraphicsPipelineState->graphicsPipesStates.push_back(left_colorBlendState);
                perViewGraphicsPipelineState->graphicsPipesStates.push_back(right_colorBlendState);

                *itr = perViewGraphicsPipelineState;
            }
        }
    }
};

vsg::ref_ptr<vsg::Node> createTextureQuad(const vsg::vec3& origin, const vsg::vec3& horizontal, const vsg::vec3& vertical, vsg::ref_ptr<vsg::Data> imageData, uint32_t mipmapLevelsHints = 0)
{

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // load shaders
    vsg::ref_ptr<vsg::ShaderStage> vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    vsg::ref_ptr<vsg::ShaderStage> fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return {};
    }

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    };

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's DispatchTraversal
    };

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // colour data
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},    // tex coord data
    };

    vsg::GraphicsPipelineStates pipelineStates{
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        vsg::RasterizationState::create(),
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(),
        vsg::DepthStencilState::create()};

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    // create texture image and associated DescriptorSets and binding
    auto sampler = vsg::Sampler::create();
    sampler->maxLod = mipmapLevelsHints;
    auto texture = vsg::DescriptorImage::create(sampler, imageData, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{texture});
    auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, vsg::DescriptorSets{descriptorSet});

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindGraphicsPipeline);
    scenegraph->add(bindDescriptorSets);

    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
        {origin,
         origin + horizontal,
         origin + horizontal + vertical,
         origin + vertical});

    auto colors = vsg::vec3Array::create(
        {{1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f}});

    float left = 0.0f;
    float right = 1.0f;
    float top = (imageData->getLayout().origin == vsg::TOP_LEFT) ? 0.0f : 1.0f;
    float bottom = (imageData->getLayout().origin == vsg::TOP_LEFT) ? 1.0f : 0.0f;
    auto texcoords = vsg::vec2Array::create(
        {{left, bottom},
         {right, bottom},
         {right, top},
         {left, top}}); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
         2, 3, 0}); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{vertices, colors, texcoords}));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(6, 1, 0, 0, 0));

    // add drawCommands to transform
    scenegraph->addChild(drawCommands);

    return scenegraph;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    double eyeSeperation = 0.06;
    double screenDistance = 0.75;
    double screenWidth = 0.55;
    double screenHorizontalResolution = 1920;

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "Anaglyphic Sterep";
    windowTraits->fullscreen = true;
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);

    vsg::vec3 offset(0.0f, 0.0f, 0.0f);
    arguments.read("--offset", offset.x, offset.z);

    vsg::Path leftImageFilename, rightImageFilename;
    arguments.read({"-s", "--stereo-pair"}, leftImageFilename, rightImageFilename);

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    uint32_t leftMask = 0x1;
    uint32_t rightMask = 0x2;

    vsg::ref_ptr<vsg::Node> vsg_scene;
    if (!leftImageFilename.empty() && !rightImageFilename.empty())
    {
        auto leftImage = vsg::read_cast<vsg::Data>(leftImageFilename, options);
        auto rightImage = vsg::read_cast<vsg::Data>(rightImageFilename, options);

        if (leftImage && rightImage)
        {
            vsg::vec3 origin(0.0f, 0.0f, 0.0f);
            vsg::vec3 horizontal(leftImage->width(), 0.0f, 0.0f);
            vsg::vec3 vertical(0.0f, 0.0f, leftImage->height());

            auto leftQuad = createTextureQuad(origin - offset * 0.5f, horizontal, vertical, leftImage);
            auto rightQuad = createTextureQuad(origin + offset * 0.5f, horizontal, vertical, rightImage);

            auto maskGroup = vsg::MaskGroup::create();
            maskGroup->addChild(leftMask, leftQuad);
            maskGroup->addChild(rightMask, rightQuad);

            vsg_scene = maskGroup;
        }
    }
    else
    {
        vsg::Path filename = "models/lz.vsgt";
        if (argc > 1) filename = arguments[1];

        vsg_scene = vsg::read_cast<vsg::Node>(filename, options);
    }

    if (!vsg_scene)
    {
        std::cout << "Unable to load model." << std::endl;
        return 1;
    }

    // ColorBlendState needs to be overriden per View, so remove existing instances in the scene graph
    ReplaceColorBlendState removeColorBlendState;
    vsg_scene->accept(removeColorBlendState);


    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create windows." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    uint32_t width = window->extent2D().width;
    uint32_t height = window->extent2D().height;

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel(vsg_scene->getObject<vsg::EllipsoidModel>("EllipsoidModel"));
    if (ellipsoidModel)
    {
        double horizonMountainHeight = 0.0;
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
    }

    auto master_camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));
    double shear = (eyeSeperation/screenWidth) * 0.8; // quick hack to get convergance roughly conincident with the trackball center.

    // create the left eye camera
    auto left_relative_perspective = vsg::RelativeProjection::create(perspective, vsg::translate(-shear, 0.0, 0.0));
    auto left_relative_view = vsg::RelativeView::create(lookAt, vsg::translate(-0.5*eyeSeperation, 0.0, 0.0));
    auto left_camera = vsg::Camera::create(left_relative_perspective, left_relative_view, vsg::ViewportState::create(window->extent2D()));

    // create the left eye camera
    auto right_relative_perspective = vsg::RelativeProjection::create(perspective, vsg::translate(shear, 0.0, 0.0));
    auto right_relative_view = vsg::RelativeView::create(lookAt, vsg::translate(0.5*eyeSeperation, 0.0, 0.0));
    auto right_camera = vsg::Camera::create(right_relative_perspective, right_relative_view, vsg::ViewportState::create(window->extent2D()));

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // add event handlers, in the order we wish event to be handled.
    viewer->addEventHandler(vsg::Trackball::create(master_camera, ellipsoidModel));

    auto renderGraph = vsg::RenderGraph::create(window);

    auto left_view = vsg::View::create(left_camera, vsg_scene);
    left_view->mask = leftMask;
    renderGraph->addChild(left_view);

    // clear the depth buffer before view2 gets rendered
    VkClearAttachment depth_attachment{VK_IMAGE_ASPECT_DEPTH_BIT, 1, VkClearValue{1.0f, 0.0f}};
    VkClearRect rect{right_camera->getRenderArea(), 0, 1};
    auto clearAttachments = vsg::ClearAttachments::create(vsg::ClearAttachments::Attachments{depth_attachment}, vsg::ClearAttachments::Rects{rect});
    renderGraph->addChild(clearAttachments);

    auto right_view = vsg::View::create(right_camera, vsg_scene);
    right_view->mask = rightMask;
    renderGraph->addChild(right_view);

    auto commandGraph = vsg::CommandGraph::create(window);
    commandGraph->addChild(renderGraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});


    viewer->compile();

    // rendering main loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        double lookDistance = vsg::length(lookAt->center - lookAt->eye);
        double horizontalSeperation = 0.5*eyeSeperation;
        horizontalSeperation *= (lookDistance / screenDistance);

        left_relative_view->matrix = vsg::translate(horizontalSeperation, 0.0, 0.0);
        right_relative_view->matrix = vsg::translate(-horizontalSeperation, 0.0, 0.0);

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
