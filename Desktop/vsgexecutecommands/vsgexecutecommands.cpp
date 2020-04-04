#include <vsg/all.h>

/* <editor-fold desc="MIT License">

Copyright(c) 2020 Julien Valentin

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */


#include <iostream>


int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    auto debugLayer = arguments.read({"--debug","-d"});
    auto apiDumpLayer = arguments.read({"--api","-a"});
    auto [width, height] = arguments.value(std::pair<uint32_t, uint32_t>(800, 600), {"--window", "-w"});
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // load shaders
    vsg::ref_ptr<vsg::ShaderStage> vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    vsg::ref_ptr<vsg::ShaderStage> fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout<<"Could not create shaders."<<std::endl;
        return 1;
    }

    // read texture image
    vsg::Path textureFile("textures/lz.vsgb");
    auto textureData = vsg::read_cast<vsg::Data>(vsg::findFile(textureFile, searchPaths));
    if (!textureData)
    {
        std::cout<<"Could not read texture file : "<<textureFile<<std::endl;
        return 1;
    }

    vsg::DescriptorSetLayoutBindings descriptorBindings
    {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    };

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::ref_ptr< vsg::GraphicsPipeline > graphicsPipelinepass1;

    {
        // set up graphics pipeline
        vsg::PushConstantRanges pushConstantRanges
        {
            {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's DispatchTraversal
        };

        vsg::VertexInputState::Bindings vertexBindingsDescriptions
        {
            VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
            VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
            VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
        };

        vsg::VertexInputState::Attributes vertexAttributeDescriptions
        {
            VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
            VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // colour data
            VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},    // tex coord data
        };

        vsg::GraphicsPipelineStates pipelineStates
        {
            vsg::VertexInputState::create( vertexBindingsDescriptions, vertexAttributeDescriptions ),
            vsg::InputAssemblyState::create(),
            vsg::RasterizationState::create(),
            vsg::MultisampleState::create(),
            vsg::ColorBlendState::create(),
            vsg::DepthStencilState::create()
        };

        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
        graphicsPipelinepass1 = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates,0);
    }

    auto bindGraphicsPipeline1 = vsg::BindGraphicsPipeline::create(graphicsPipelinepass1);
    // create texture image and associated DescriptorSets and binding
    auto texture = vsg::DescriptorImage::create(vsg::Sampler::create(), textureData, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{texture});
    auto bindDescriptorSets1 = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelinepass1->getPipelineLayout(), 0, vsg::DescriptorSets{descriptorSet});

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraphsecondary = vsg::StateGroup::create();

    scenegraphsecondary->add(bindGraphicsPipeline1);
    scenegraphsecondary->add(bindDescriptorSets1);

    // set up model transformation node
    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT

    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
    {
        {-0.5f, -0.5f, 0.0f},
        {0.5f,  -0.5f, 0.05f},
        {0.5f, 0.5f, 0.0f},
        {-0.5f, 0.5f, 0.0f},
        {-0.5f, -0.5f, -0.5f},
        {0.5f,  -0.5f, -0.5f},
        {0.5f, 0.5f, -0.5},
        {-0.5f, 0.5f, -0.5}
    }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto colors = vsg::vec3Array::create(
    {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
    }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto texcoords = vsg::vec2Array::create(
    {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f},
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f}
    }); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto indices = vsg::ushortArray::create(
    {
        0, 1, 2,
        2, 3, 0,
        4, 5, 6,
        6, 7, 4
    }); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE


    // setup geometry
    auto drawCommandspass1 = vsg::Commands::create();
    drawCommandspass1->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{vertices, colors, texcoords}));
    drawCommandspass1->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommandspass1->addChild(vsg::DrawIndexed::create(12, 1, 0, 0, 0));

    scenegraphsecondary->addChild(transform);
    transform->addChild(drawCommandspass1);

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    vsg::ref_ptr<vsg::WindowTraits> traits(new vsg::WindowTraits());
    traits->width = width;
    traits->height = height;
    //traits->shareWindow = shareWindow;
    traits->debugLayer = debugLayer;
    traits->apiDumpLayer = apiDumpLayer;
    //traits->allocator = allocator;

    traits->device = vsg::Device::create(traits);

    vsg::ref_ptr<vsg::WindowTraits> traits2(new vsg::WindowTraits());
    traits2->width = width;
    traits2->height = height;
    //traits->shareWindow = shareWindow;
    traits2->debugLayer = debugLayer;
    traits2->apiDumpLayer = apiDumpLayer;
    traits2->device = traits->device ;
    vsg::ref_ptr<vsg::Window> window1(vsg::Window::create(traits));
    vsg::ref_ptr<vsg::Window> window2(vsg::Window::create(traits2));

    if (!window1)
    {
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window1);
    viewer->addWindow(window2);

    // camera related details
    auto viewport = vsg::ViewportState::create(VkExtent2D{width, height});
    auto perspective = vsg::Perspective::create(60.0, static_cast<double>(width) / static_cast<double>(height), 0.1, 10.0);
    auto lookAt = vsg::LookAt::create(vsg::dvec3(1.0, 1.0, 1.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);


    auto seccommandGraph1 = vsg::createCommandGraphForView(window1, camera, scenegraphsecondary, VK_COMMAND_BUFFER_LEVEL_SECONDARY, 0);

    auto scenegraphwin1 = vsg::Group::create();
    auto  pass1= vsg::ExecuteCommands::create();
    pass1 ->addCommandGraph(seccommandGraph1 );

    auto scenegraphwin2 = vsg::Group::create();
    auto  pass2= vsg::ExecuteCommands::create();
    pass2 ->addCommandGraph(seccommandGraph1 );

    scenegraphwin1->addChild(pass1);
    scenegraphwin2->addChild(pass2);

    auto commandGraphwin1 = vsg::createCommandGraphForView(window1, camera, scenegraphwin1, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 0, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    auto commandGraphwin2 = vsg::createCommandGraphForView(window2, camera, scenegraphwin2, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 0, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraphwin1,commandGraphwin2});

    // compile the Vulkan objects
    viewer->compile();

    // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

    // main frame loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        // animate the transform
        float time = std::chrono::duration<float, std::chrono::seconds::period>(viewer->getFrameStamp()->time - viewer->start_point()).count();
        transform->setMatrix(vsg::rotate(time * vsg::radians(90.0f), vsg::vec3(0.0f, 0.0, 1.0f)));

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
