#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <iomanip>
#include <fstream>
#include <yaml-cpp/yaml.h>
#include <Eigen/Dense>

#include "MaterialRegistry.h"
#include "MechanicalMaterial.h"
#include "FieldManager.h"
#include "FieldRegistry.h"

int main(int argc, char* argv[]) {
    std::cout << "[test_constitutive] Starting single point JSON-driven test..." << std::endl;

    std::string inputPath = "input_path.json";
    if (argc > 1) {
        inputPath = argv[1];
    }

    // 1. 读取 input_path.json
    YAML::Node inputNode;
    try {
        inputNode = YAML::LoadFile(inputPath);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load input file: " << inputPath << ", error: " << e.what() << std::endl;
        return 1;
    }

    if (!inputNode["parameters"] || (!inputNode["F_path"] && !inputNode["strain_path"])) {
        std::cerr << "Invalid input JSON structure! Must contain 'parameters' and either 'F_path' or 'strain_path'." << std::endl;
        return 1;
    }

    // 2. Initialize constitutive parameters
    YAML::Node matNode = inputNode["parameters"];
    std::string matType = matNode["Type"] ? matNode["Type"].as<std::string>() : "J2VoceLemaitre";
    auto matBase = PDCommon::Material::MaterialRegistry::getInstance().createMaterial(matType, "Steel_Notched");
    if (!matBase) {
        std::cerr << "Failed to create material of type: " << matType << std::endl;
        return 1;
    }
    auto mat = dynamic_cast<PDCommon::Material::MechanicalMaterial*>(matBase.get());
    if (!mat) {
        std::cerr << "Material of type: " << matType << " is not a MechanicalMaterial!" << std::endl;
        return 1;
    }
    mat->initialize(matNode);

    // 3. Initialize FieldManager and bind variables
    PDCommon::Field::FieldManager fm;
    mat->allocateStateVariables(fm);
    fm.resizeAll(1);
    mat->bindStateVariables(fm);

    // 4. Parse loading path (F_path or strain_path)
    bool useFPath = false;
    size_t numSteps = 0;
    YAML::Node FPathNode;
    YAML::Node strainPathNode;

    if (inputNode["F_path"]) {
        FPathNode = inputNode["F_path"];
        numSteps = FPathNode.size();
        useFPath = true;
        std::cout << "[test_constitutive] Loaded " << numSteps << " steps of deformation gradient path (F_path)." << std::endl;
    } else if (inputNode["strain_path"]) {
        strainPathNode = inputNode["strain_path"];
        numSteps = strainPathNode.size();
        std::cout << "[test_constitutive] Loaded " << numSteps << " steps of strain path." << std::endl;
    } else {
        std::cerr << "Invalid input JSON! Must contain either 'F_path' or 'strain_path'." << std::endl;
        return 1;
    }

    std::ofstream outFile("cpp_results.json");
    if (!outFile.is_open()) {
        std::cerr << "Failed to open output json file!" << std::endl;
        return 1;
    }

    outFile << "{\n  \"steps\": [\n";

    // Parse large deformation flag
    bool isLarge = false;
    if (matNode["LargeDeformation"]) {
        isLarge = matNode["LargeDeformation"].as<bool>();
    }

    for (size_t step = 0; step < numSteps; ++step) {
        Eigen::Matrix3d F = Eigen::Matrix3d::Identity();
        Eigen::Matrix3d epsMat = Eigen::Matrix3d::Zero();

        if (useFPath) {
            YAML::Node stepF = FPathNode[step];
            if (stepF.size() != 9) {
                std::cerr << "Step " << step << " F must have exactly 9 components!" << std::endl;
                return 1;
            }
            F << stepF[0].as<double>(), stepF[1].as<double>(), stepF[2].as<double>(),
                 stepF[3].as<double>(), stepF[4].as<double>(), stepF[5].as<double>(),
                 stepF[6].as<double>(), stepF[7].as<double>(), stepF[8].as<double>();

            // Compute strain tensor based on large deformation flag
            if (isLarge) {
                epsMat = 0.5 * (F.transpose() * F - Eigen::Matrix3d::Identity());
            } else {
                epsMat = 0.5 * (F + F.transpose() - 2.0 * Eigen::Matrix3d::Identity());
            }
        } else {
            YAML::Node stepStrain = strainPathNode[step];
            if (stepStrain.size() != 6) {
                std::cerr << "Step " << step << " strain must have exactly 6 components!" << std::endl;
                return 1;
            }

            // Reconstruct 3x3 strain tensor from 6-component Voigt input
            epsMat(0, 0) = stepStrain[0].as<double>();
            epsMat(1, 1) = stepStrain[1].as<double>();
            epsMat(2, 2) = stepStrain[2].as<double>();
            epsMat(0, 1) = epsMat(1, 0) = stepStrain[3].as<double>();
            epsMat(1, 2) = epsMat(2, 1) = stepStrain[4].as<double>();
            epsMat(2, 0) = epsMat(0, 2) = stepStrain[5].as<double>();

            F = Eigen::Matrix3d::Identity() + epsMat;
        }

        // Compute stress. stateMode=0 updates state.
        Eigen::Matrix3d PK1 = mat->ComputePK1Stress(F, 0, 0, nullptr);

        // Extract state variables dynamically
        double damage = fm.hasField("Damage_Trial") ? fm.getFieldAs<double>("Damage_Trial")->dataPtr()[0] : 0.0;
        double eqps = fm.hasField("EqPlasticStrain_Trial") ? fm.getFieldAs<double>("EqPlasticStrain_Trial")->dataPtr()[0] : 0.0;
        double vonMises = fm.hasField("VonMisesStress") ? fm.getFieldAs<double>("VonMisesStress")->dataPtr()[0] : 0.0;

        // Commit state
        fm.executeAllRegisteredSwaps();
        mat->commitState();

        // Write to JSON
        outFile << "    {\n";
        outFile << "      \"step\": " << step << ",\n";
        // Output strain tensor as 9-component row-major
        outFile << "      \"strain\": ["
                << std::scientific << std::setprecision(17)
                << epsMat(0,0) << ", " << epsMat(0,1) << ", " << epsMat(0,2) << ", "
                << epsMat(1,0) << ", " << epsMat(1,1) << ", " << epsMat(1,2) << ", "
                << epsMat(2,0) << ", " << epsMat(2,1) << ", " << epsMat(2,2) << "],\n";
        // Output stress tensor as 9-component row-major
        outFile << "      \"stress\": ["
                << std::scientific << std::setprecision(17)
                << PK1(0,0) << ", " << PK1(0,1) << ", " << PK1(0,2) << ", "
                << PK1(1,0) << ", " << PK1(1,1) << ", " << PK1(1,2) << ", "
                << PK1(2,0) << ", " << PK1(2,1) << ", " << PK1(2,2) << "],\n";
        outFile << "      \"von_mises\": " << std::setprecision(17) << vonMises << ",\n";
        outFile << "      \"eqp\": " << std::setprecision(17) << eqps << ",\n";
        outFile << "      \"damage\": " << std::setprecision(17) << damage << "\n";
        if (step + 1 < numSteps) {
            outFile << "    },\n";
        } else {
            outFile << "    }\n";
        }
    }

    outFile << "  ]\n}\n";
    outFile.close();

    std::cout << "[test_constitutive] Simulation completed. Results written to cpp_results.json" << std::endl;
    return 0;
}