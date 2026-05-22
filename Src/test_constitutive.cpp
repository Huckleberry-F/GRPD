#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <iomanip>
#include <fstream>
#include <yaml-cpp/yaml.h>
#include <Eigen/Dense>

#include "J2VoceLemaitreMat.h"
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

    // 2. 初始化本构参�?    YAML::Node matNode = inputNode["parameters"];
    auto mat = std::make_unique<PDCommon::Material::J2VoceLemaitreMat>("Steel_Notched");
    mat->initialize(matNode);

    // 3. 初始�?FieldManager 空间并绑�?    PDCommon::Field::FieldManager fm;
    mat->allocateStateVariables(fm);
    fm.resizeAll(1);
    mat->bindStateVariables(fm);

    // 4. 解析加载路径 (优先 F_path，其�?strain_path)
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

    // 预先解析大变形标识（仅需读取一次）
    bool isLarge = false;
    if (matNode["LargeDeformation"]) {
        isLarge = matNode["LargeDeformation"].as<bool>();
    }

    for (size_t step = 0; step < numSteps; ++step) {
        Eigen::Matrix3d F = Eigen::Matrix3d::Identity();
        // 应变张量统一提升到循环顶层，保证两种输入路径均可写入 9 分量
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

            // 根据大变形配置反推应变张�?            if (isLarge) {
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

            // �?6 分量 Voigt 输入重建对称 3x3 应变张量
            epsMat(0, 0) = stepStrain[0].as<double>();
            epsMat(1, 1) = stepStrain[1].as<double>();
            epsMat(2, 2) = stepStrain[2].as<double>();
            epsMat(0, 1) = epsMat(1, 0) = stepStrain[3].as<double>();
            epsMat(1, 2) = epsMat(2, 1) = stepStrain[4].as<double>();
            epsMat(2, 0) = epsMat(0, 2) = stepStrain[5].as<double>();

            F = Eigen::Matrix3d::Identity() + epsMat;
        }

        // 计算应力。stateMode �?0 表示正常更新状态的模式�?        Eigen::Matrix3d PK1 = mat->ComputePK1Stress(F, 0, 0);

        // 提取状态变�?        double damage = fm.getFieldAs<double>("Damage_Trial")->dataPtr()[0];
        double eqps = fm.getFieldAs<double>("EqPlasticStrain_Trial")->dataPtr()[0];
        double vonMises = fm.getFieldAs<double>("VonMisesStress")->dataPtr()[0];

        // 提交状�?        fm.executeAllRegisteredSwaps();
        mat->commitState();

        // 写入 JSON
        outFile << "    {\n";
        outFile << "      \"step\": " << step << ",\n";
        // 应变张量 9 分量行主序输�? [ε₁₁, ε₁₂, ε₁₃, ε₂₁, ε₂₂, ε₂₃, ε₃₁, ε₃₂, ε₃₃]
        outFile << "      \"strain\": ["
                << std::scientific << std::setprecision(12)
                << epsMat(0,0) << ", " << epsMat(0,1) << ", " << epsMat(0,2) << ", "
                << epsMat(1,0) << ", " << epsMat(1,1) << ", " << epsMat(1,2) << ", "
                << epsMat(2,0) << ", " << epsMat(2,1) << ", " << epsMat(2,2) << "],\n";
        // 应力张量 9 分量行主序输�? [σ₁₁, σ₁₂, σ₁₃, σ₂₁, σ₂₂, σ₂₃, σ₃₁, σ₃₂, σ₃₃]
        outFile << "      \"stress\": ["
                << std::scientific << std::setprecision(12)
                << PK1(0,0) << ", " << PK1(0,1) << ", " << PK1(0,2) << ", "
                << PK1(1,0) << ", " << PK1(1,1) << ", " << PK1(1,2) << ", "
                << PK1(2,0) << ", " << PK1(2,1) << ", " << PK1(2,2) << "],\n";
        outFile << "      \"von_mises\": " << vonMises << ",\n";
        outFile << "      \"eqp\": " << eqps << ",\n";
        outFile << "      \"damage\": " << damage << "\n";
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