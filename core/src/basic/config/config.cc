#include "config.hh"

#include <fstream>
#include <set>

namespace {

bool isUnsignedArrayIndex(const std::string& segment) {
    if (segment.empty()) return false;
    for (char ch : segment) {
        if (ch < '0' || ch > '9') return false;
    }
    return true;
}

bool pathStartsWith(const std::vector<std::string>& path, const std::vector<std::string>& prefix) {
    if (prefix.size() > path.size()) return false;
    for (std::size_t index = 0; index < prefix.size(); ++index) {
        if (path[index] != prefix[index]) return false;
    }
    return true;
}

nlohmann::json& ensureJsonPath(nlohmann::json& root, const std::vector<std::string>& path) {
    nlohmann::json* current = &root;
    for (const auto& segment : path) {
        if (current->is_array()) {
            std::size_t index = static_cast<std::size_t>(std::stoull(segment));
            current = &(*current)[index];
            continue;
        }
        current = &(*current)[segment];
    }
    return *current;
}

} // namespace

// ---- AFS_ConfigValueTypeName -------------------------------------------------
std::string AFS_ConfigValueTypeName(AFS_ConfigValueType type) {
    switch (type) {
    case AFS_ConfigValueType::String:
        return "string";
    case AFS_ConfigValueType::UnsignedInteger:
        return "unsigned integer";
    case AFS_ConfigValueType::Number:
        return "number";
    case AFS_ConfigValueType::Boolean:
        return "boolean";
    }
    return "unknown";
}

// ---- AFS_Config --------------------------------------------------------------
bool AFS_Config::loadFromFile(const std::filesystem::path& path, std::string& error) {
    std::ifstream file(path);
    if (!file.is_open()) {
        error = "cannot open config file: " + path.string();
        return false;
    }

    nlohmann::json parsed;
    try {
        file >> parsed;
    } catch (const nlohmann::json::parse_error& ex) {
        error = std::string("parse config JSON failed: ") + ex.what();
        return false;
    }
    if (!parsed.is_object()) {
        error = "config root must be a JSON object";
        return false;
    }

    path_ = path;
    root_ = std::move(parsed);
    return true;
}

bool AFS_Config::save(std::string& error) const {
    if (path_.empty()) {
        error = "config path is empty";
        return false;
    }
    std::ofstream output(path_);
    if (!output.is_open()) {
        error = "cannot write config file: " + path_.string();
        return false;
    }
    output << root_.dump(2) << '\n';
    return true;
}

const nlohmann::json* AFS_Config::valueAt(const std::vector<std::string>& path) const {
    const nlohmann::json* current = &root_;
    for (const auto& segment : path) {
        if (current->is_object()) {
            auto it = current->find(segment);
            if (it == current->end()) return nullptr;
            current = &(*it);
            continue;
        }
        if (current->is_array()) {
            std::size_t index = 0;
            try {
                index = static_cast<std::size_t>(std::stoull(segment));
            } catch (const std::exception&) {
                return nullptr;
            }
            if (index >= current->size()) return nullptr;
            current = &(*current)[index];
            continue;
        }
        return nullptr;
    }
    return current;
}

bool AFS_Config::updateValue(const std::vector<std::string>& path, const nlohmann::json& value,
                             std::string& error) {
    ensureObjectRoot();
    try {
        ensureJsonPath(root_, path) = value;
    } catch (const std::exception& ex) {
        error = std::string("failed to update config path: ") + ex.what();
        return false;
    }
    return true;
}

void AFS_Config::ensureObjectRoot() {
    if (!root_.is_object()) root_ = nlohmann::json::object();
}

// ---- AFS_ConfigManager -------------------------------------------------------
AFS_ConfigManager& AFS_ConfigManager::instance() {
    static AFS_ConfigManager manager;
    return manager;
}

void AFS_ConfigManager::registerSchema(AFS_ConfigSchema schema, AFS_ConfigApplyFn apply) {
    for (auto& existing : schemas_) {
        if (existing.module == schema.module && existing.path == schema.path) {
            if (apply) appliers_[schema.module] = std::move(apply);
            existing = std::move(schema);
            return;
        }
    }

    if (apply) appliers_[schema.module] = std::move(apply);
    schemas_.push_back(std::move(schema));
}

bool AFS_ConfigManager::loadFromFile(const std::filesystem::path& path, std::string& error) {
    return config_.loadFromFile(path, error);
}

bool AFS_ConfigManager::save(std::string& error) const {
    return config_.save(error);
}

AFS_ConfigUpdateResult AFS_ConfigManager::updateValue(const std::vector<std::string>& path,
                                                      const nlohmann::json& value) {
    AFS_ConfigUpdateResult result;
    if (!config_.updateValue(path, value, result.error)) return result;

    result.ok = true;
    result.affected_modules = affectedModulesForPath(path);
    return result;
}

bool AFS_ConfigManager::applyModule(const std::string& module, std::string& error) const {
    auto it = appliers_.find(module);
    if (it == appliers_.end()) return true;
    return it->second(config_, error);
}

bool AFS_ConfigManager::applyModules(const std::vector<std::string>& modules,
                                     std::string& error) const {
    for (const auto& module : modules) {
        if (!applyModule(module, error)) return false;
    }
    return true;
}

std::vector<std::string>
AFS_ConfigManager::affectedModulesForPath(const std::vector<std::string>& path) const {
    std::set<std::string> affected;
    for (const auto& schema : schemas_) {
        if (pathStartsWith(path, schema.path) || pathStartsWith(schema.path, path)) {
            affected.insert(schema.module);
            continue;
        }
        if (schema.is_array && path.size() >= schema.path.size() + 1) {
            std::vector<std::string> parent(path.begin(), path.begin() + schema.path.size());
            if (parent == schema.path && isUnsignedArrayIndex(path[schema.path.size()])) {
                affected.insert(schema.module);
            }
        }
    }
    return {affected.begin(), affected.end()};
}
