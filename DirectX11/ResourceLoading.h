#pragma once

#include <d3d11_1.h>
#include <cstdint>
#include <string>
#include <unordered_set>

// What identifies the contents of a file without reading it: the file itself,
// its size, and the times the file system updates on every write. Renaming
// the file or a folder above it keeps them, so a Mod folder that is renamed
// stays the same file. Compares unequal after any write, as git's index does.
struct FileIdentity
{
	uint64_t volume = 0;
	uint64_t index = 0;
	uint64_t size = 0;
	int64_t last_write = 0;
	int64_t change = 0;

	bool operator==(const FileIdentity &other) const;
};

// From an open handle, or by opening the file for its attributes only:
bool GetFileIdentity(HANDLE file, FileIdentity *identity);
bool GetFileIdentity(const wchar_t *path, FileIdentity *identity);

// A whole file read into memory. Reading into a buffer kept for the next
// file is much faster than faulting the pages of a mapped view in one by one
// while a D3D create call copies from it, and the buffer is warm in the
// cache for that copy. A file of several MB is read by thread pool threads
// as well, before the constructor returns.
class FileContents
{
public:
	explicit FileContents(const wchar_t *path);
	~FileContents();

	FileContents(const FileContents&) = delete;
	FileContents& operator=(const FileContents&) = delete;

	bool valid() const { return data != nullptr; }
	// Of the file as read: nothing could write to it while it was open
	bool identity(FileIdentity *identity) const;

	const uint8_t *data = nullptr;
	size_t size = 0;
	DWORD error = 0;

private:
	uint8_t *buffer = nullptr;
	size_t capacity = 0;
	FileIdentity file_identity;
	bool has_identity = false;
};

// Once a frame: frees the buffer of files too big for the kept ones once it
// has gone unused for some seconds
void ReleaseIdleReadBuffers();

enum class SharedResourceLoader {
	DDS,
	WIC,
	BUFFER,
};

// Identifies a GPU resource created from a file by the file's contents and
// everything that affected its creation, so that two [Resource] sections
// pointing at identical files end up with the same ID3D11Resource:
struct SharedResourceKey
{
	ID3D11Device *device = nullptr;
	SharedResourceLoader loader = SharedResourceLoader::DDS;
	uint32_t bind_flags = 0;
	uint32_t misc_flags = 0;
	// DDS forceSRGB or the WIC loader flags:
	uint32_t load_flags = 0;
	// Buffers only, as created after the ini overrides:
	uint32_t byte_width = 0;
	uint32_t structure_stride = 0;

	// Filled in by FindSharedFileResource() from the data the resource is
	// created from. The crc32c only narrows down the candidates:
	uint64_t size = 0;
	uint32_t content = 0;

	bool operator==(const SharedResourceKey &other) const;
};

// The registry holds a reference of its own on every shared resource until
// it is released, so that a hit is never a dangling pointer. A hit means the
// data is byte for byte identical to the file an earlier resource with the
// same key was created from, and the returned resource is AddRef()ed for the
// caller. On a miss the key is complete and can be passed to
// ShareFileResource(), together with the path of the file, once the resource
// has been created from the same data. path_identity is that of the file the
// data came from, when known, so the next config can find it by
// FindRetainedFileResource().
ID3D11Resource* FindSharedFileResource(SharedResourceKey &key, const void *data, size_t size,
		const std::wstring *path = nullptr, const FileIdentity *path_identity = nullptr);
void ShareFileResource(const SharedResourceKey &key, const std::wstring &path, ID3D11Resource *resource,
		const FileIdentity *path_identity = nullptr);

// The resources survive a config reload, so a Mod that has not changed keeps
// its textures and buffers instead of loading them from disk again. Given the
// key without its size and contents (those are implied by the identity), this
// finds the resource created from this very file, unchanged since, without
// reading it. A hit is AddRef()ed for the caller.
ID3D11Resource* FindRetainedFileResource(const SharedResourceKey &key, const std::wstring &path,
		const FileIdentity &identity);

// Called once a reloaded config has parsed every [Resource] section, with the
// lower case path of each file they load: keeps the resources used since the
// previous reload whose file is still in the config, and drops the rest.
void RetainSharedFileResources(const std::unordered_set<std::wstring> &lower_paths);
// Drops the registry's references, all of them or those on one device:
void ReleaseSharedFileResources(ID3D11Device *device = nullptr);

// Appends a resource file to the list the next session reads into the OS
// file cache ahead of the file's first use:
void RecordResourceFileUse(const std::wstring &path);
void StartResourceFilePrefetch();
void StopResourceFilePrefetch();
