
#include <GL/glew.h>
#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <iostream>
#include <vector>
#include <random>

#include "CellRulesShader.h"
#include "CellMeshingShader.h"
#include "CellRenderShader.h"
#include "Automaton.h"

int main(int argc, char* argv[]) 
{
    srand(time(nullptr));
    const int WIN_WIDTH = 900;
    const int WIN_HEIGHT = 900;

	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window* window = SDL_CreateWindow("3D Cellular Automata", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(window);
    glewInit();

    int width = 128;
    int height = 128;
    int depth = 128;
    int maxDimension = width;
    if (height > maxDimension)
        maxDimension = height;
    if (depth > maxDimension)
        maxDimension = depth;

    /* This lambda returns a function to generate random cells within a cube of
       width w. Each cell has a probability of 1/n of being alive. */
    auto randomCubeSeed = [](int n, int w)
    {
        return [n, w](uint32_t* cells, int width, int height, int depth)
        {
            for (int z = (depth - w) / 2; z < (depth + w) / 2; z++)
            {
                for (int y = (height - w) / 2; y < (height + w) / 2; y++)
                {
                    for (int x = (width - w) / 2; x < (width + w) / 2; x++)
                    {
                        if (rand() % n == 0)
                            cells[x + y * width + z * width * height] = 1;
                    }
                }
            }
        };
    };

    // List of different automata names, rules, color schemes, and seed functions
    std::vector<Automaton> automata
    ({
        Automaton
        (
            "Plasma", 
            "B 4,5,10,14,21,25 / S 6,8,12,13,18,25 / 6",
            { 0, 0x7e00ff, 0xff00f0, 0xb079ff, 0x3fa3ff, 0x00bcff },
            randomCubeSeed(2, 6)
        ),
        Automaton
        (
            "Regular Growth", 
            "B 5,13,21,22 / S 4,5,6,12,13,22,25 / 3",
            { 0, 0xffff00, 0x7f7f00 },
            randomCubeSeed(2, 10)
        ),
        Automaton
        (
            "Dissolve", 
            "B 5,6,7,9,10,13,15,16,18,19,20,21,23 / S 8,10,12,15,17,21,26 / 4",
            { 0, 0x2c2c2c, 0xa6a6a6, 0x666666 },
            randomCubeSeed(5, 128)
        ),
        Automaton
        (
            "Fungus", 
            "B 5,6,7,9 / S 3,6,9,10 / 6",
            { 0, 0xaaa25c, 0x3d2800, 0x584317, 0x74612c, 0x8f8143 },
            randomCubeSeed(2, 20)
        ),
        Automaton
        (
            "Cube", 
            "B 1,2,3,4,5,6,7,11,16,17,21,25,26 / S 2,5,7,12,15,16,17,18,19,21,22,23,24 / 5",
            { 0, 0xf24c3d, 0xf29727, 0x22a699, 0xf2be22 },
            randomCubeSeed(1, 2)
        ),
        Automaton
        (
            "Tetradecahedron", 
            "B 2,3,4,11,23,24 / S 1,2,14,16,17,20,24 / 9",
            { 
                0, 0x22a699, 0xf29727, 0xf24c3d, 0xf2be22, 0x1d5b79, 
                0x468b97, 0xef6262, 0xf3aa60 
            },
            randomCubeSeed(1, 2)
        ),
        Automaton
        (
            "Life 3D", 
            "B 7,11,12,13 / S 3,4,6,7,8,9,10,11 / 20",
            { 
                0, 0x510000, 0xff0000, 0xf50003, 0xeb0005, 0xe00007, 0xd60009, 
                0xcc000a, 0xc2000b, 0xb8000c, 0xaf000c, 0xa5000c, 0x9b000c, 
                0x91000c, 0x88000b, 0x7e000a, 0x750009, 0x6c0007, 0x630005, 
                0x5a0003 
            },
            randomCubeSeed(2, 10)
        ),
        Automaton
        (
            "Heartbeat", 
            "B 4,5,6 / S 1 / 25",
            { 
                0, 0xff0000, 0x1800ff, 0x0052ff, 0x0074ff, 0x008eff, 0x00a2ff, 
                0x00b3ff, 0x00c2ff, 0x00d1e8, 0x00dfb9, 0x00eb88, 0x00f656, 
                0x12ff00, 0x12ff00, 0x67f100, 0x8de200, 0xa7d200, 0xbdc100, 
                0xcfb000, 0xde9d00, 0xea8900, 0xf47300, 0xfa5a00, 0xfe3c00 
            },
            randomCubeSeed(1, 2)
        ),
        Automaton
        (
            "Space Cars", 
            "B 4,5 / S 10 / 15", 
            {  
                0, 0xff0000, 0xa200ff, 0xff008e, 0xff8c15, 0xfffc00, 0xffea00, 
                0xffd800, 0xffc500, 0xffb100, 0xff9d00, 0xff8900, 0xff7300, 
                0xff5a00, 0xff3c00 
            },
            randomCubeSeed(10, 20)
        ),
        Automaton
        (
            "Persist", 
            "B 5,8,9 / S 2,11 / 2",
            { 0, 0x956bb0 },
            randomCubeSeed(11, width)
        ),
    });

    // Three shader stages: evaluate automata logic, generate mesh, render (vertex + fragment)
    CellRulesShader cellRulesShader(width, height, depth, automata[0].rule);
    CellMeshingShader cellMeshingShader(width, height, depth);
    CellRenderShader cellRenderShader(width, height, depth, cellMeshingShader.getMeshSSBO());

    uint32_t oldTime = SDL_GetTicks();
    bool quit = false;
    // Angles for camera (controlled with mouse movement)
    float yAngle = 0.0f;
    float xAngle = 0.0f;
    // Camera field of view in degrees (controlled with +/- keys)
    float fovAngle = 70.0f;
    // Half second timer for printing framerate approximation
    float fpsTimer = 0.0f;
    // Time between frames of simulation (controlled with mouse wheel)
    float frameDelay = 0.01f;
    // When simTimer > frameDelay, evaluate automaton logic
    float simTimer = 0.0f;
    // Current automaton rules being used (controlled with left/right keys)
    int automatonID = 0;
    while (!quit) 
    {
        uint32_t newTime = SDL_GetTicks();
        float delta = (newTime - oldTime) / 1000.0f;
        oldTime = newTime;

        fpsTimer += delta;
        if (fpsTimer > 0.5f)
        {
            std::cout << "FPS: " << static_cast<int>(1.0f / delta) << std::endl;
            fpsTimer = 0.0f;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) 
        {
            if (event.type == SDL_QUIT)
                quit = true;
            if (event.type == SDL_MOUSEWHEEL)
            {
                frameDelay += -event.wheel.preciseY * frameDelay * delta * 10.0f;
                if (frameDelay < 0.01f)
                    frameDelay = 0.01f;
            }
            if (event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDL_KeyCode::SDLK_SPACE)
                {
                    // Fetch current cell grid from GPU, add cells using seed function, send cell grid back to GPU
                    cellRulesShader.fetchGPUCells();
                    automata[automatonID].seedFunction(cellRulesShader.getCells(), width, height, depth);
                    cellRulesShader.updateGPUCells();
                }
                else if (event.key.keysym.sym == SDL_KeyCode::SDLK_LEFT)
                {
                    automatonID--;
                    if (automatonID < 0)
                        automatonID = automata.size() - 1;
                    cellRulesShader.setRule(automata[automatonID].rule);
                }
                else if (event.key.keysym.sym == SDL_KeyCode::SDLK_RIGHT)
                {
                    automatonID++;
                    if (automatonID > automata.size() - 1)
                        automatonID = 0;
                    cellRulesShader.setRule(automata[automatonID].rule);
                }
                else if (event.key.keysym.sym == SDL_KeyCode::SDLK_EQUALS)
                {
                    fovAngle -= 10.0f;
                    if (fovAngle < 10.0f)
                        fovAngle = 10.0f;
                }
                else if (event.key.keysym.sym == SDL_KeyCode::SDLK_MINUS)
                {
                    fovAngle += 10.0f;
                    if (fovAngle > 120.0f)
                        fovAngle = 120.0f;
                }
            }
        }

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        int mx, my;
        SDL_GetMouseState(&mx, &my);
        yAngle = (static_cast<float>(mx) / WIN_WIDTH) * glm::radians(360.0f);
        xAngle = (static_cast<float>(my) / WIN_HEIGHT) * glm::radians(180.0f) - glm::radians(90.0f);

        glm::mat4 view =
            glm::translate(glm::vec3(0.0f, 0.0f, -1.3f * maxDimension)) *
            glm::rotate(xAngle, glm::vec3(1.0f, 0.0f, 0.0f)) *
            glm::rotate(yAngle, glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::translate(glm::vec3(-width / 2.0f, -height / 2.0f, -depth / 2.0f));
        glm::mat4 projection = glm::perspective(glm::radians(fovAngle), static_cast<float>(WIN_WIDTH) / WIN_HEIGHT, 0.1f, 1000.0f);

        glm::mat4 mvp = projection * view;

        simTimer += delta;
        if (simTimer > frameDelay)
        {
            simTimer = 0.0f;
            // Represents one timestep of cellular automaton
            cellRulesShader.simulate();
        }
        /* 6 meshing/rendering stages, one for each side of each cube.
           For example, at i=0, every cube's left side is meshed and rendered. */
        for (int i = 0; i < 6; i++)
        {
            // Meshing shader takes in cell state buffer as input and outputs to vertex buffer
            cellMeshingShader.meshCells(cellRulesShader.getCellSSBO(), i, automata[automatonID].colorScheme);
            // Render shader uses glDrawArrays to render the mesh created by the meshing shader
            cellRenderShader.renderMesh(mvp, i);
        }

        SDL_GL_SwapWindow(window);
        SDL_Delay(1);
    }

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

	return 0;
}
