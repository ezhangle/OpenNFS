//
//  main.cpp
//  FCE-To-OBJ
//
//  Created by Amrik Sadhra on 27/01/2015.
//

#define TINYOBJLOADER_IMPLEMENTATION

#ifdef VULKAN_BUILD
#define GLFW_INCLUDE_VULKAN

#include "Renderer/vkRenderer.h"

#endif

#include <cstdlib>
#include <string>
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "Config.h"
#include "Util/Logger.h"
#include "Loaders/trk_loader.h"
#include "Loaders/car_loader.h"
#include "Loaders/music_loader.h"
#include "Physics/Car.h"
#include "Renderer/Renderer.h"
#include "RaceNet/TrainingGround.h"

class OpenNFS {
public:
    explicit OpenNFS(std::shared_ptr<Logger> &onfs_logger) : logger(onfs_logger) {
        InitDirectories();
        PopulateAssets();

        if (Config::get().vulkanRender) {
#ifdef VULKAN_BUILD
            vkRenderer renderer;
            renderer.run();
#else
            ASSERT(false, "This build of OpenNFS was not compiled with Vulkan support!");
#endif
        } else if (Config::get().trainingMode) {
            train();
        } else {
            run();
        }
    }

    void run() {
        LOG(INFO) << "OpenNFS Version " << ONFS_VERSION;

        // Must initialise OpenGL here as the Loaders instantiate meshes which create VAO's
        ASSERT(InitOpenGL(Config::get().resX, Config::get().resY, "OpenNFS v" + ONFS_VERSION), "OpenGL init failed.");

        AssetData loadedAssets = {
                NFS_3, Config::get().car,
                NFS_3, Config::get().track
        };

        if (Config::get().car != DEFAULT_CAR) {
            loadedAssets.carTag = FindCarByName(Config::get().car);
        }

        /*------- Render --------*/
        while (loadedAssets.trackTag != UNKNOWN) {
            /*------ ASSET LOAD ------*/
            //Load Track Data
            std::shared_ptr<ONFSTrack> track = TrackLoader::LoadTrack(loadedAssets.trackTag, loadedAssets.track);
            //Load Car data from unpacked NFS files
            std::shared_ptr<Car> car = CarLoader::LoadCar(loadedAssets.carTag, loadedAssets.car);

            //Load Music
            //MusicLoader musicLoader("F:\\NFS3\\nfs3_modern_base_eng\\gamedata\\audio\\pc\\atlatech");

            Renderer renderer(window, logger, installedNFS, track, car);
            loadedAssets = renderer.Render();
        }

        // Close OpenGL window and terminate GLFW
        glfwTerminate();
    }

    void train() {
        LOG(INFO) << "OpenNFS Version " << ONFS_VERSION << " (GA Training Mode)";

        // Must initialise OpenGL here as the Loaders instantiate meshes which create VAO's
        ASSERT(InitOpenGL(Config::get().resX, Config::get().resY, "OpenNFS v" + ONFS_VERSION + " (GA Training Mode)"),
               "OpenGL init failed.");

        AssetData trainingAssets = {
                NFS_3, Config::get().car,
                NFS_3, Config::get().track
        };

        if (Config::get().car != DEFAULT_CAR) {
            trainingAssets.carTag = FindCarByName(Config::get().car);
        }

        /*------ ASSET LOAD ------*/
        //Load Track Data
        std::shared_ptr<ONFSTrack> track = TrackLoader::LoadTrack(trainingAssets.trackTag, trainingAssets.track);
        //Load Car data from unpacked NFS files
        std::shared_ptr<Car> car = CarLoader::LoadCar(trainingAssets.carTag, trainingAssets.car);

        auto trainingGround = TrainingGround(Config::get().populationSize, Config::get().nGenerations,
                                             Config::get().nTicks, track, car, logger, window);
    }

private:
    GLFWwindow *window;

    std::shared_ptr<Logger> logger;

    std::vector<NeedForSpeed> installedNFS;

    static void glfwError(int id, const char *description) {
        LOG(WARNING) << description;
    }

    static void window_size_callback(GLFWwindow *window, int width, int height) {
        Config::get().resX = width;
        Config::get().resY = height;
    }

    bool InitOpenGL(int resolutionX, int resolutionY, const std::string &windowName) {
        // Initialise GLFW
        ASSERT(glfwInit(), "GLFW Init failed.\n");
        glfwSetErrorCallback(&glfwError);

        // TODO: Disable MSAA for now until texture array adds padding
        //wglfwWindowHint(GLFW_SAMPLES, 4);

#ifdef __APPLE__
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Appease the OSX Gods
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#else
        // TODO: If we fail to create a GL context on Windows, fall back to not requesting any (Keiiko Bug #1)
        //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
#endif

        window = glfwCreateWindow(resolutionX, resolutionY, windowName.c_str(), nullptr, nullptr);

        if (window == nullptr) {
            LOG(WARNING) << "Failed to create a GLFW window.";
            getchar();
            glfwTerminate();
            return false;
        }
        glfwMakeContextCurrent(window);

        glfwSetWindowSizeCallback(window, window_size_callback);

        // Initialize GLEW
        glewExperimental = GL_TRUE; // Needed for core profile

        if (glewInit() != GLEW_OK) {
            LOG(WARNING) << "Failed to initialize GLEW";
            getchar();
            glfwTerminate();
            return false;
        }

        // Ensure we can capture the escape key being pressed below
        glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
        // Set the mouse at the center of the screen
        glfwPollEvents();

        // Dark blue background
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        // Enable depth test
        glEnable(GL_DEPTH_TEST);
        // Accept fragment if it closer to the camera than the former one
        glDepthFunc(GL_LESS);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        GLint texture_units, max_array_texture_layers;
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texture_units);
        glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_array_texture_layers);
        LOG(INFO) << "Max Texture Units: " << texture_units;
        LOG(INFO) << "Max Array Texture Layers: " << max_array_texture_layers;
        LOG(INFO) << "OpenGL Initialisation successful";

        return true;
    }

    void InitDirectories() {
        if (!(boost::filesystem::exists(CAR_PATH))) {
            boost::filesystem::create_directories(CAR_PATH);
        }
        if (!(boost::filesystem::exists(TRACK_PATH))) {
            boost::filesystem::create_directories(TRACK_PATH);
        }
    }

    void PopulateAssets() {
        using namespace boost::filesystem;

        path basePath(RESOURCE_PATH);
        bool hasLanes = false;
        bool hasMisc = false;
        bool hasSfx = false;

        for (directory_iterator itr(basePath); itr != directory_iterator(); ++itr) {
            NeedForSpeed currentNFS;
            currentNFS.tag = UNKNOWN;

            if (itr->path().filename().string().find(ToString(NFS_2_SE)) != std::string::npos) {
                currentNFS.tag = NFS_2_SE;

                std::stringstream trackBasePathStream;
                trackBasePathStream << itr->path().string() << NFS_2_SE_TRACK_PATH;
                std::string trackBasePath(trackBasePathStream.str());
                ASSERT(exists(trackBasePath),
                       "NFS 2 Special Edition track folder: " << trackBasePath << " is missing.");

                for (directory_iterator trackItr(trackBasePath); trackItr != directory_iterator(); ++trackItr) {
                    if (trackItr->path().filename().string().find(".TRK") != std::string::npos) {
                        currentNFS.tracks.emplace_back(trackItr->path().filename().replace_extension("").string());
                    }
                }

                std::stringstream carBasePathStream;
                carBasePathStream << itr->path().string() << NFS_2_SE_CAR_PATH;
                std::string carBasePath(carBasePathStream.str());
                ASSERT(exists(carBasePath), "NFS 2 Special Edition car folder: " << carBasePath << " is missing.");

                // TODO: Work out where NFS2 SE Cars are stored
            } else if (itr->path().filename().string().find(ToString(NFS_2)) != std::string::npos) {
                currentNFS.tag = NFS_2;

                std::stringstream trackBasePathStream;
                trackBasePathStream << itr->path().string() << NFS_2_TRACK_PATH;
                std::string trackBasePath(trackBasePathStream.str());
                ASSERT(exists(trackBasePath), "NFS 2 track folder: " << trackBasePath << " is missing.");

                for (directory_iterator trackItr(trackBasePath); trackItr != directory_iterator(); ++trackItr) {
                    if (trackItr->path().filename().string().find(".TRK") != std::string::npos) {
                        currentNFS.tracks.emplace_back(trackItr->path().filename().replace_extension("").string());
                    }
                }

                std::stringstream carBasePathStream;
                carBasePathStream << itr->path().string() << NFS_2_CAR_PATH;
                std::string carBasePath(carBasePathStream.str());
                ASSERT(exists(carBasePath), "NFS 2 car folder: " << carBasePath << " is missing.");

                for (directory_iterator carItr(carBasePath); carItr != directory_iterator(); ++carItr) {
                    if (carItr->path().filename().string().find(".GEO") != std::string::npos) {
                        currentNFS.cars.emplace_back(carItr->path().filename().replace_extension("").string());
                    }
                }
            } else if (itr->path().filename().string().find(ToString(NFS_3_PS1)) != std::string::npos) {
                currentNFS.tag = NFS_3_PS1;

                for (directory_iterator trackItr(itr->path().string()); trackItr != directory_iterator(); ++trackItr) {
                    if (trackItr->path().filename().string().find(".TRK") != std::string::npos) {
                        currentNFS.tracks.emplace_back(trackItr->path().filename().replace_extension("").string());
                    }
                }

                for (directory_iterator carItr(itr->path().string()); carItr != directory_iterator(); ++carItr) {
                    if (carItr->path().filename().string().find(".GEO") != std::string::npos) {
                        currentNFS.cars.emplace_back(carItr->path().filename().replace_extension("").string());
                    }
                }
            } else if (itr->path().filename().string().find(ToString(NFS_3)) != std::string::npos) {
                currentNFS.tag = NFS_3;

                std::stringstream trackBasePathStream;
                trackBasePathStream << itr->path().string() << NFS_3_TRACK_PATH;
                std::string trackBasePath(trackBasePathStream.str());
                ASSERT(exists(trackBasePath), "NFS 3 Hot Pursuit track folder: " << trackBasePath << " is missing.");

                for (directory_iterator trackItr(trackBasePath); trackItr != directory_iterator(); ++trackItr) {
                    currentNFS.tracks.emplace_back(trackItr->path().filename().string());
                }

                std::stringstream carBasePathStream;
                carBasePathStream << itr->path().string() << NFS_3_CAR_PATH;
                std::string carBasePath(carBasePathStream.str());
                ASSERT(exists(carBasePath), "NFS 3 Hot Pursuit car folder: " << carBasePath << " is missing.");

                for (directory_iterator carItr(carBasePath); carItr != directory_iterator(); ++carItr) {
                    if (carItr->path().filename().string().find("traffic") == std::string::npos) {
                        currentNFS.cars.emplace_back(carItr->path().filename().string());
                    }
                }

                carBasePathStream << "traffic/";
                for (directory_iterator carItr(carBasePathStream.str()); carItr != directory_iterator(); ++carItr) {
                    currentNFS.cars.emplace_back("traffic/" + carItr->path().filename().string());
                }

                carBasePathStream << "pursuit/";
                for (directory_iterator carItr(carBasePathStream.str()); carItr != directory_iterator(); ++carItr) {
                    if (carItr->path().filename().string().find("PURSUIT") == std::string::npos) {
                        currentNFS.cars.emplace_back("traffic/pursuit/" + carItr->path().filename().string());
                    }
                }
            } else if (itr->path().filename().string().find(ToString(NFS_4)) != std::string::npos) {
                currentNFS.tag = NFS_4;

                std::stringstream trackBasePathStream;
                trackBasePathStream << itr->path().string() << NFS_4_TRACK_PATH;
                std::string trackBasePath(trackBasePathStream.str());
                ASSERT(exists(trackBasePath), "NFS 4 High Stakes track folder: " << trackBasePath << " is missing.");

                for (directory_iterator trackItr(trackBasePath); trackItr != directory_iterator(); ++trackItr) {
                    currentNFS.tracks.emplace_back(trackItr->path().filename().string());
                }

                std::stringstream carBasePathStream;
                carBasePathStream << itr->path().string() << NFS_4_CAR_PATH;
                std::string carBasePath(carBasePathStream.str());
                ASSERT(exists(carBasePath), "NFS 4 High Stakes car folder: " << carBasePath << " is missing.");

                for (directory_iterator carItr(carBasePath); carItr != directory_iterator(); ++carItr) {
                    if (carItr->path().filename().string().find("TRAFFIC") == std::string::npos) {
                        currentNFS.cars.emplace_back(carItr->path().filename().string());
                    }
                }

                carBasePathStream << "TRAFFIC/";
                for (directory_iterator carItr(carBasePathStream.str()); carItr != directory_iterator(); ++carItr) {
                    if ((carItr->path().filename().string().find("CHOPPERS") == std::string::npos) &&
                        (carItr->path().filename().string().find("PURSUIT") == std::string::npos)) {
                        currentNFS.cars.emplace_back("TRAFFIC/" + carItr->path().filename().string());
                    }
                }

                carBasePathStream << "CHOPPERS/";
                for (directory_iterator carItr(carBasePathStream.str()); carItr != directory_iterator(); ++carItr) {
                    currentNFS.cars.emplace_back("TRAFFIC/CHOPPERS/" + carItr->path().filename().string());
                }

                carBasePathStream.str(std::string());
                carBasePathStream << itr->path().string() << NFS_4_CAR_PATH << "TRAFFIC/" << "PURSUIT/";
                for (directory_iterator carItr(carBasePathStream.str()); carItr != directory_iterator(); ++carItr) {
                    currentNFS.cars.emplace_back("TRAFFIC/PURSUIT/" + carItr->path().filename().string());
                }
            } else if (itr->path().filename().string().find("lanes") != std::string::npos) {
                hasLanes = true;
                continue;
            } else if (itr->path().filename().string().find("misc") != std::string::npos) {
                hasMisc = true;
                continue;
            } else if (itr->path().filename().string().find("sfx") != std::string::npos) {
                hasSfx = true;
                continue;
            } else {
                LOG(WARNING) << "Unknown folder in resources directory: " << itr->path().filename().string();
                continue;
            }
            installedNFS.emplace_back(currentNFS);
        }

        ASSERT(hasLanes, "Missing \'lanes\' folder in resources directory");
        ASSERT(hasMisc, "Missing \'misc\' folder in resources directory");
        ASSERT(hasSfx, "Missing \'sfx\' folder in resources directory");
        ASSERT(installedNFS.size(), "No Need for Speed games detected in resources directory");

        for (auto nfs : installedNFS) {
            LOG(INFO) << "Detected: " << ToString(nfs.tag);
        }
    }

    NFSVer FindCarByName(const std::string &car_name) {
        std::vector<NFSVer> possibleNFS;
        NFSVer carNFSVersion;

        for (auto nfs : installedNFS) {
            for (const auto &car : nfs.cars) {
                if (car == car_name) {
                    possibleNFS.emplace_back(nfs.tag);
                }
            }
        }

        ASSERT(possibleNFS.size(), "Specified car '" << car_name << "' does not exist across any NFS.");

        if (possibleNFS.size() == 1) {
            carNFSVersion = possibleNFS[0];
        } else {
            LOG(INFO) << "Selected car exists in multiple NFS versions. Please select desired version: ";
            for (uint8_t nfs_Idx = 0; nfs_Idx < possibleNFS.size(); ++nfs_Idx) {
                LOG(INFO) << (int) nfs_Idx << ". " << ToString(possibleNFS[nfs_Idx]);
            }
            std::string line;
            int choice = 0;
            while ((std::cin >> choice)&&!(choice >= 0 && choice < possibleNFS.size())) {
                LOG(INFO) << "Invalid selection, try again.";
            }
            carNFSVersion = possibleNFS[choice];
        }
        return carNFSVersion;
    }
};

int main(int argc, char **argv) {
    Config::get().InitFromCommandLine(argc, argv);
    std::shared_ptr<Logger> logger = std::make_shared<Logger>();

    try {
        OpenNFS game(logger);
    } catch (const std::runtime_error &e) {
        LOG(WARNING) << e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
