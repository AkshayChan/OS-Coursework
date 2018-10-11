/*
 * TAR File-system Driver
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (3)
 */

/*
 * STUDENT NUMBER: s1558717
 */
#include "tarfs.h"
#include <infos/kernel/log.h>

using namespace infos::fs;
using namespace infos::drivers;
using namespace infos::drivers::block;
using namespace infos::kernel;
using namespace infos::util;
using namespace tarfs;


/**
 * TAR files contain header data encoded as octal values in ASCII.  This function
 * converts this terrible representation into a real unsigned integer.
 *
 * You DO NOT need to modify this function.
 *
 * @param data The (null-terminated) ASCII data containing an octal number.
 * @return Returns an unsigned integer number, corresponding to the input data.
 */
static inline unsigned int octal2ui(const char *data)
{
	// Current working value.
	unsigned int value = 0;

	// Length of the input data.
	int len = strlen(data);

	// Starting at i = 1, with a factor of one.
	int i = 1, factor = 1;
	while (i < len) {
		// Extract the current character we're working on (backwards from the end).
		char ch = data[len - i];

		// Add the value of the character, multipled by the factor, to
		// the working value.
		value += factor * (ch - '0');
		
		// Increment the factor by multiplying it by eight.
		factor *= 8;
		
		// Increment the current character position.
		i++;
	}

	// Return the current working value.
	return value;
}

// The structure that represents the header block present in
// TAR files.  A header block occurs before every file, this
// this structure must EXACTLY match the layout as described
// in the TAR file format description.
namespace tarfs {
	struct posix_header
	{                              /* byte offset */
  	char name[100];               /*   0 */
  	char mode[8];                 /* 100 */
  	char uid[8];                  /* 108 */
  	char gid[8];                  /* 116 */
  	char size[12];                /* 124 */
  	char mtime[12];               /* 136 */
  	char chksum[8];               /* 148 */
  	char typeflag;                /* 156 */
  	char linkname[100];           /* 157 */
  	char magic[6];                /* 257 */
  	char version[2];              /* 263 */
  	char uname[32];               /* 265 */
  	char gname[32];               /* 297 */
  	char devmajor[8];             /* 329 */
  	char devminor[8];             /* 337 */
  	char prefix[155];             /* 345 */
                                /* 500 */
	}__packed;
}

/**
 * Reads the contents of the file into the buffer, from the specified file offset.
 * @param buffer The buffer to read the data into.
 * @param size The size of the buffer, and hence the number of bytes to read.
 * @param off The offset within the file.
 * @return Returns the number of bytes read into the buffer.
 */


int TarFSFile::pread(void* buffer, size_t size, off_t off)
{
	if (off >= this->size()) return 0;

	// TO BE FILLED IN
	
	// buffer is a pointer to the buffer that should receive the data.
	// size is the amount of data to read from the file.
	// off is the zero-based offset within the file to start reading from.

	//If the size to br read is 0 or the size of the file is 0, return 0
	if(size == 0 || this->size() == 0) return 0;
	unsigned int size_check = off + size;

	//If the file is smaller than bytes we need, only read till end of file
	if (this->size() < off + size) {
		size_check = this->size();
	}
	
	//Here we find the block right before the block that contains our offset
	unsigned int current_block = 0;
 	for (unsigned int i = 0; (i*512) < off; i++) {
		current_block = i; 
	}

	//syslog.messagef(LogLevel::DEBUG, "off is %lu", off);	
	//syslog.messagef(LogLevel::DEBUG, "size is %lu", size);	
	//syslog.messagef(LogLevel::DEBUG, "current_block is %lu", current_block);

	//Now we start reading an entire block into a temporary buffer 
	int bytes_read = 0;

	//Declare our void buffer as an int buffer
	uint8_t * rbuffer = (uint8_t *) buffer;
	uint8_t* temp = new uint8_t[_owner.block_device().block_size()];
	
	//Read one block per time into temporary buffer and make sure our current block doesn't cross the block we have to read till 
	for (unsigned int i = current_block; (i*512) < size_check; i++) {
		_owner.block_device().read_blocks(temp, _file_start_block + i, 1);
		unsigned int reader = i*512;
	
		//We're in the blocks that have bytes to be read, but make sure we start reading after off in the start block
		// and stop reading before size check in the end block
		for (int j = 0; j < 512; j ++) {
			if( reader + j >=off && reader + j < size_check) {
				rbuffer[bytes_read] = temp[j]; 		
	                     	bytes_read++;
			}
		}
	}

	//syslog.messagef(LogLevel::DEBUG, "bytes read are %lu", bytes_read);
	
	return bytes_read;
	
}

/**
 * Reads all the file headers in the TAR file, and builds an in-memory
 * representation.
 * @return Returns the root TarFSNode that corresponds to the TAR file structure.
 */

TarFSNode* TarFS::build_tree()
{
	// Create the root node.
	TarFSNode *root = new TarFSNode(NULL, "", *this);
	
	//Header of the file we have 
	struct posix_header *header = (struct posix_header *) new char[block_device().block_size()];

	//Check for if we have reached the end of the file
	uint8_t * check = new uint8_t[block_device().block_size()];
	size_t nr_blocks = block_device().block_count();

	TarFSNode *parent;

	for (unsigned int current_block = 0; current_block < nr_blocks;) {
		
		//Read the first block of file, either header or the first of the last two zero blocks
		//Check if we have reached the end of the archive
		block_device().read_blocks(header, current_block, 1);
		block_device().read_blocks(check, current_block + 1, 1);
		if (is_zero_block((uint8_t*) header) && is_zero_block(check)) {
			return root;
		}

		//Checking the file size, does it divide perfectly? 
		//If it does, number of blocks are simple, else add a block.
		unsigned int size = octal2ui(header->size);
		if (size % 512 == 0) {
			size = size/512;
		}
		else {
			size = size/512 + 1;
		}		
		
		//Getting the path to the file
		parent = root;
		String path_to_file = String(header->name);	
		List<String> path_list = path_to_file.split('/', true);

		//Iterating through all the files/directories in the name, checking if the file exists as a child 
		int element = 0;
		int count = path_list.count();
		while(element < count) {
			//If the parent does not have this child
			if(!parent->get_child(path_list.at(element))) {
				TarFSNode *child = new TarFSNode(parent, path_list.at(element), *this);
				parent->add_child(path_list.at(element), child);
				
				//If this element we are looking at is a file, not a directory
				if (element == count - 1) { 				
					child->set_block_offset(current_block);
					child->size(octal2ui(header->size));		
				}
				element++;
			}
			else { 	
				//Simply assign the parent to the child
				parent = (TarFSNode*) (parent->get_child(path_list.at(element))); 	
				element++;
			}
		}
		
		//Add a 1 for the header
		current_block = current_block + size + 1;
	}

	// You must read the TAR file, and build a tree of TarFSNodes that represents each file present in the archive.
	return root;
}


/**
 * Returns the size of this TarFS File
 */
unsigned int TarFSFile::size() const
{
	// TO BE FILLED IN
	return octal2ui(_hdr->size);
}

/* --- YOU DO NOT NEED TO CHANGE ANYTHING BELOW THIS LINE --- */

/**
 * Mounts a TARFS filesystem, by pre-building the file system tree in memory.
 * @return Returns the root node of the TARFS filesystem.
 */
PFSNode *TarFS::mount()
{
	// If the root node has not been generated, then build it.
	if (_root_node == NULL) {
		_root_node = build_tree();
	}

	// Return the root node.
	return _root_node;
}

/**
 * Constructs a TarFS File object, given the owning file system and the block
 */
TarFSFile::TarFSFile(TarFS& owner, unsigned int file_header_block)
: _hdr(NULL),
_owner(owner),
_file_start_block(file_header_block),
_cur_pos(0)
{
	// Allocate storage for the header.
	_hdr = (struct posix_header *) new char[_owner.block_device().block_size()];
	
	// Read the header block into the header structure.
	_owner.block_device().read_blocks(_hdr, _file_start_block, 1);
	
	// Increment the starting block for file data.
	_file_start_block++;
}

TarFSFile::~TarFSFile()
{
	// Delete the header structure that was allocated in the constructor.
	delete _hdr;
}

/**
 * Releases any resources associated with this file.
 */
void TarFSFile::close()
{
	// Nothing to release.
}

/**
 * Reads the contents of the file into the buffer, from the current file offset.
 * The current file offset is advanced by the number of bytes read.
 * @param buffer The buffer to read the data into.
 * @param size The size of the buffer, and hence the number of bytes to read.
 * @return Returns the number of bytes read into the buffer.
 */
int TarFSFile::read(void* buffer, size_t size)
{
	// Read can be seen as a special case of pread, that uses an internal
	// current position indicator, so just delegate actual processing to
	// pread, and update internal state accordingly.

	// Perform the read from the current file position.
	int rc = pread(buffer, size, _cur_pos);

	// Increment the current file position by the number of bytes that was read.
	// The number of bytes actually read may be less than 'size', so it's important
	// we only advance the current position by the actual number of bytes read.
	_cur_pos += rc;

	// Return the number of bytes read.
	return rc;
}

/**
 * Moves the current file pointer, based on the input arguments.
 * @param offset The offset to move the file pointer either 'to' or 'by', depending
 * on the value of type.
 * @param type The type of movement to make.  An absolute movement moves the
 * current file pointer directly to 'offset'.  A relative movement increments
 * the file pointer by 'offset' amount.
 */
void TarFSFile::seek(off_t offset, SeekType type)
{
	// If this is an absolute seek, then set the current file position
	// to the given offset (subject to the file size).  There should
	// probably be a way to return an error if the offset was out of bounds.
	if (type == File::SeekAbsolute) {
		_cur_pos = offset;
	} else if (type == File::SeekRelative) {
		_cur_pos += offset;
	}
	if (_cur_pos >= size()) {
		_cur_pos = size() - 1;
	}
}

TarFSNode::TarFSNode(TarFSNode *parent, const String& name, TarFS& owner) : PFSNode(parent, owner), _name(name), _size(0), _has_block_offset(false), _block_offset(0)
{
}

TarFSNode::~TarFSNode()
{
}

/**
 * Opens this node for file operations.
 * @return 
 */
File* TarFSNode::open()
{
	// This is only a file if it has been associated with a block offset.
	if (!_has_block_offset) {
		return NULL;
	}

	// Create a new file object, with a header from this node's block offset.
	return new TarFSFile((TarFS&) owner(), _block_offset);
}

/**
 * Opens this node for directory operations.
 * @return 
 */
Directory* TarFSNode::opendir()
{
	return new TarFSDirectory(*this);
}

/**
 * Attempts to retrieve a child node of the given name.
 * @param name
 * @return 
 */
PFSNode* TarFSNode::get_child(const String& name)
{
	TarFSNode *child;

	// Try to find the given child node in the children map, and return
	// NULL if it wasn't found.
	if (!_children.try_get_value(name.get_hash(), child)) {
		return NULL;
	}

	return child;
}

/**
 * Creates a subdirectory in this node.  This is a read-only file-system,
 * and so this routine does not need to be implemented.
 * @param name
 * @return 
 */
PFSNode* TarFSNode::mkdir(const String& name)
{
	// DO NOT IMPLEMENT
	return NULL;
}

/**
 * A helper routine that updates this node with the offset of the block
 * that contains the header of the file that this node represents.
 * @param offset The block offset that corresponds to this node.
 */
void TarFSNode::set_block_offset(unsigned int offset)
{
	_has_block_offset = true;
	_block_offset = offset;
}

/**
 * A helper routine that adds a child node to the internal children
 * map of this node.
 * @param name The name of the child node.
 * @param child The actual child node.
 */
void TarFSNode::add_child(const String& name, TarFSNode *child)
{
	_children.add(name.get_hash(), child);
}

TarFSDirectory::TarFSDirectory(TarFSNode& node) : _entries(NULL), _nr_entries(0), _cur_entry(0)
{
	_nr_entries = node.children().count();
	_entries = new DirectoryEntry[_nr_entries];

	int i = 0;
	for (const auto& child : node.children()) {
		_entries[i].name = child.value->name();
		_entries[i++].size = child.value->size();
	}
}

TarFSDirectory::~TarFSDirectory()
{
	delete _entries;
}

bool TarFSDirectory::read_entry(infos::fs::DirectoryEntry& entry)
{
	if (_cur_entry < _nr_entries) {
		entry = _entries[_cur_entry++];
		return true;
	} else {
		return false;
	}
}

void TarFSDirectory::close()
{

}

static Filesystem *tarfs_create(VirtualFilesystem& vfs, Device *dev)
{
	if (!dev->device_class().is(BlockDevice::BlockDeviceClass)) return NULL;
	return new TarFS((BlockDevice &) * dev);
}

RegisterFilesystem(tarfs, tarfs_create);
