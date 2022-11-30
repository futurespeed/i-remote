#include <string>
#include <map>
#include <list>
#include <stdio.h>

#define String std::string
#define Map std::map
#define List std::list

class SceneConfig
{
public:
    SceneConfig(String code, String name);
    // ~SceneConfig();
    void keyMapPut(String key, String code);
    void longKeyMapPut(String key, String code);
    String getKeyValue(String key);
    String getLongKeyValue(String key);

private:
    String code;
    String name;
    Map<String, String> keyMap;
    Map<String, String> longKeyMap;
};

class IRemoteConfig
{
public:
    void addScene(SceneConfig scene);
    void cleanScene();
    SceneConfig getScene(uint8_t idx);

private:
    String code;
    String name;
    List<SceneConfig> scenes;
};

SceneConfig::SceneConfig(String code, String name)
{
    this->code = code;
    this->name = name;
};

void SceneConfig::keyMapPut(String key, String value)
{
    this->keyMap[key] = value;
}

void SceneConfig::longKeyMapPut(String key, String value)
{
    this->longKeyMap[key] = value;
}

String SceneConfig::getKeyValue(String key)
{
    Map<String, String>::iterator ite = this->keyMap.find(key);
    if (ite == this->keyMap.end())
        return String("");
    return ite->second;
}

String SceneConfig::getLongKeyValue(String key)
{
    Map<String, String>::iterator ite = this->longKeyMap.find(key);
    if (ite == this->longKeyMap.end())
        return String("");
    return ite->second;
}

void IRemoteConfig::addScene(SceneConfig scene)
{
    this->scenes.push_back(scene);
}

void IRemoteConfig::cleanScene()
{
    this->scenes.clear();
}

SceneConfig IRemoteConfig::getScene(uint8_t idx)
{
    List<SceneConfig>::iterator ite = this->scenes.begin();
    uint8_t cnt = idx;
    while(cnt--) ite++;
    return *ite;
}

int main(int argc, char const *argv[])
{
    IRemoteConfig config;
    SceneConfig scene("s1", "s2");
    scene.keyMapPut("a", "b");
    scene.keyMapPut("c", "d");
    config.addScene(scene);

    SceneConfig scene2("222", "s2222");
    scene2.keyMapPut("a", "b2");
    scene2.keyMapPut("c", "d2");
    config.addScene(scene2);

    printf("%s\r\n", scene.getKeyValue("a").c_str());
    printf("%s\r\n", config.getScene(1).getKeyValue("c").c_str());
    return 0;
}
