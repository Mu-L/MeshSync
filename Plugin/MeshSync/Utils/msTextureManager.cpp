#include "pch.h"
#include "msTextureManager.h"

namespace ms {

TextureManager::TextureManager()
{
    // random seed
    std::uniform_int_distribution<> d(0, 0x70000000);
    std::mt19937 r;
    r.seed(std::random_device()());
    m_id_seed = d(r);
}

TextureManager::~TextureManager()
{
    waitTasks();
}

void TextureManager::clear()
{
    waitTasks();
    m_records.clear();
}

void TextureManager::erase(const std::string& name)
{
    m_records.erase(name);
}

int TextureManager::find(const std::string& name) const
{
    auto it = m_records.find(name);
    if (it != m_records.end())
        return it->second.texture->id;
    return -1;
}

int TextureManager::addImage(const std::string& name, int width, int height, const void *data, size_t size, TextureFormat format)
{
    auto& rec = m_records[name];
    int id = rec.texture ? rec.texture->id : genID();

    auto checksum = SumInt32(data, size);
    if (!rec.texture || rec.checksum != checksum) {
        rec.checksum = checksum;
        rec.texture = Texture::create();
        auto& tex = rec.texture;
        tex->id = id;
        tex->name = name;
        tex->format = format;
        tex->width = width;
        tex->height = height;
        tex->data.assign((const char*)data, (const char*)data + size);
        rec.dirty = true;
    }
    return id;
}

int TextureManager::addFile(const std::string& path, TextureType type)
{
    auto& rec = m_records[path];
    int id = rec.texture ?
        rec.texture->id :
        (FileExists(path.c_str()) ? genID() : -1);

    rec.waitTask();
    rec.task = std::async(std::launch::async, [this, path, type, &rec, id]() {
        auto mtime = FileMTime(path.c_str());
        if (!rec.texture || rec.mtime != mtime) {
            rec.mtime = mtime;
            rec.texture = Texture::create();
            auto& tex = rec.texture;
            auto& data = tex->data;
            if (FileToByteArray(path.c_str(), data)) {
                tex->id = id;
                tex->name = mu::GetFilename(path.c_str());
                tex->format = ms::TextureFormat::RawFile;
                tex->type = type;
                rec.dirty = true;
            }
            else {
                tex->id = -1;
            }
        }
    });
    return id;
}

std::vector<TexturePtr> TextureManager::getAllTextures()
{
    waitTasks();

    std::vector<TexturePtr> ret;
    for (auto& p : m_records)
        ret.push_back(p.second.texture);
    return std::vector<TexturePtr>();
}

std::vector<TexturePtr> TextureManager::getDirtyTextures()
{
    waitTasks();

    std::vector<TexturePtr> ret;
    for (auto& p : m_records) {
        if (p.second.dirty)
            ret.push_back(p.second.texture);
    }
    return ret;
}

void TextureManager::clearDirtyFlags()
{
    for (auto& p : m_records) {
        p.second.dirty = false;
    }
}

int TextureManager::genID()
{
    return ++m_id_seed;
}

void TextureManager::waitTasks()
{
    for (auto& p : m_records)
        p.second.waitTask();
}

void TextureManager::Record::waitTask()
{
    if (task.valid()) {
        task.wait();
        task = {};
    }
}

} // namespace ms