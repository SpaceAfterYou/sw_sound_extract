// sw_sound_extract.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <iomanip>
#include <numeric>
#include <vector>

#pragma comment(lib, "../../deps/fmod/lib/fmodex_vc.lib")

#include "../../deps/fmod/include/fmod.hpp"
#include "../../deps/fmod/include/fmod_errors.h"

template <typename B = std::string>
B read_file (std::filesystem::path const& _path) noexcept
{
	std::ifstream file{ _path, std::ios::binary };
	if (!file.is_open ()) { return B{}; }

	file.seekg (0, std::ios::end);

	B buffer (static_cast <size_t> (file.tellg ()), '\0');
	file.seekg (0, std::ios::beg);
	file.read (&buffer.front (), std::size (buffer));

	return buffer;
}

namespace sw::message
{
	void error (std::string_view const _message) noexcept
	{
		std::cout << _message << '\n';
		std::exit (EXIT_FAILURE);
	}
} // namespace sw::message

namespace sw::helper::fmodex
{
	void check_error (FMOD_RESULT const _r) noexcept
	{
		if (FMOD_OK != _r)
		{
			std::cout << " > error (" << FMOD_ErrorString (_r) << ") #" << _r << '\n';
			std::exit (EXIT_FAILURE);
		}
	}
} // sw::helper::fmodex

namespace sw::wrapper::fmodex
{
	class system final : public FMOD::System
	{
	public:
		~system (void) {
			this->close ();
			this->release ();
		}
	};

	class sound final : public FMOD::Sound
	{
	public:
		~sound (void) {
			this->release ();
		}
	};
} // namespace sw::wrapper::fmodex

namespace sw::helper::write
{
	void wave_header (std::ofstream& _file, int a2, unsigned __int8 a3, unsigned __int8 a4, int a5)
	{
		int v5;
		unsigned int v6;
		int v9;
		int Str;

		// RIFF chunk
		_file.write ("RIFF", 4u);

		// RIFF chunk size in bytes
		Str = a5 + 36;
		_file.write (reinterpret_cast <const char*> (&Str), 4u);

		// WAVE chunk
		_file.write ("WAVE", 4u);

		// fmt chunk
		_file.write ("fmt ", 4u);

		// size of fmt chunk
		Str = 16;
		_file.write (reinterpret_cast <const char*> (&Str), 4u);

		// Format = PCM
		v9 = 1;
		_file.write (reinterpret_cast <const char*> (&v9), 2u);

		// # of Channels
		v9 = a4;
		_file.write (reinterpret_cast <const char*> (&v9), 2u);

		// Sample Rate
		_file.write (reinterpret_cast <const char*> (&a2), 4u);

		// Byte rate
		v5 = a4;
		Str = (a2 * v5 * ((unsigned int)a3)) / 8;
		_file.write (reinterpret_cast <const char*> (&Str), 4u);

		// Frame size
		v6 = (v5 * ((unsigned int)a3)) / 8;
		v9 = v6;
		_file.write (reinterpret_cast <const char*> (&v9), 2u);

		// Bits per sample
		v9 = a3;
		_file.write (reinterpret_cast <const char*> (&v9), 2u);

		// data chunk
		_file.write ("data", 4u);

		// data chunk size in bytes
		Str = a5;
		_file.write (reinterpret_cast <const char*> (&Str), 4u);
	}
} // namespace sw::helper::write

class sound
{
public:
	void extract (std::vector <char> const& _file, std::filesystem::path const& _dest) noexcept
	{
		FMOD_CREATESOUNDEXINFO exinfo{ 0 };
		exinfo.cbsize = sizeof (exinfo);
		exinfo.length = std::size (_file);

		sw::wrapper::fmodex::sound* sound;
		sw::helper::fmodex::check_error (
			m_system->createSound (std::data (_file), FMOD_OPENMEMORY_POINT | FMOD_CREATESTREAM, &exinfo, reinterpret_cast <FMOD::Sound**> (&sound))
		);

		FMOD_SOUND_TYPE sound_type{ FMOD_SOUND_TYPE_UNKNOWN };
		FMOD_SOUND_FORMAT sound_format{ FMOD_SOUND_FORMAT_NONE };
		int sound_channels{ 0 };
		int sound_bits{ 0 };
		sw::helper::fmodex::check_error (sound->getFormat (&sound_type, &sound_format, &sound_channels, &sound_bits));

		int num_sub_sounds{ 0 };
		sw::helper::fmodex::check_error (sound->getNumSubSounds (&num_sub_sounds));

		for (int q{ 0 }; q < num_sub_sounds; ++q) {
			FMOD::Sound* sub_sound{ nullptr };
			sw::helper::fmodex::check_error (sound->getSubSound (q, &sub_sound));

			sw::helper::fmodex::check_error (sub_sound->seekData (0));

			char name[100]{ '\0' };
			sw::helper::fmodex::check_error (sub_sound->getName (std::data (name), 100));

			std::string path;
			path.append (name);
			path.append (".wav");

			std::cout << path << '\n';

			float frequency;
			float volume;
			float pan;
			int priority;
			sw::helper::fmodex::check_error (sub_sound->getDefaults (&frequency, &volume, &pan, &priority));

			FMOD_SOUND_TYPE type;
			FMOD_SOUND_FORMAT format;
			int channels;
			int bits;
			sw::helper::fmodex::check_error (sub_sound->getFormat (&type, &format, &channels, &bits));

			unsigned int length;
			sw::helper::fmodex::check_error (sub_sound->getLength (&length, FMOD_TIMEUNIT_PCM));

			const unsigned int buffer_size{ static_cast <uint32_t> (bits * sound_channels * length) / 8 };

			std::vector <char> buffer;
			buffer.reserve (buffer_size);

			uint32_t read{ 0 };
			sw::helper::fmodex::check_error (sub_sound->readData (std::data (buffer), buffer_size, &read));

			std::ofstream file{ _dest / path, std::ofstream::binary };
			if (file) {
				sw::helper::write::wave_header (file, (signed __int64)frequency, bits, sound_channels, read);
				file.write (std::data (buffer), read);
			}
		}
	}

	sound (void) noexcept
	{
		sw::helper::fmodex::check_error (FMOD::System_Create (reinterpret_cast <FMOD::System**> (&m_system)));

		uint32_t version{ 0 };
		sw::helper::fmodex::check_error (m_system->getVersion (&version));

		std::cout << " > fmodex header version 0x" << std::hex << FMOD_VERSION << '\n';
		std::cout << " > fmodex dll version 0x" << std::hex << version << std::dec << '\n';

		if (version < FMOD_VERSION) {
			sw::message::error ("fmodex dll version too low");
		}

		sw::helper::fmodex::check_error (m_system->init (32, 0, nullptr));
	}
private:
	sw::wrapper::fmodex::system* m_system{ nullptr };
};

class extract
{
public:
	static void arc (
		sound& _sound,
		std::filesystem::path const& _path,
		std::filesystem::path const& _dest
	) noexcept {
		std::ifstream file{ _path, std::ios::binary };

		const constexpr size_t id_package = sizeof ("VISIONPACKAGE\0") - 1;
		char package[id_package];
		file.read (package, sizeof (package));

		short version{ -1 };
		file.read (reinterpret_cast <char*> (&version), sizeof (version));

		long files{ -1 };
		file.read (reinterpret_cast <char*> (&files), sizeof (files));

		long name_size{ -1 };
		file.read (reinterpret_cast <char*> (&name_size), sizeof (name_size));

		std::cout
			<< "----------------------------------------------\n"
			<< std::setw (sizeof (package)) << std::left << "package"
			<< " | "
			<< std::setw (sizeof ("version")) << std::left << "version"
			<< " | "
			<< std::setw (sizeof ("files")) << std::left << "files"
			<< " | "
			<< std::setw (sizeof ("name_size")) << std::left << "name_size\n";

		std::cout
			<< "----------------------------------------------\n"
			<< std::setw (sizeof (package)) << std::left << package
			<< " | "
			<< std::setw (sizeof ("version")) << std::left << version
			<< " | "
			<< std::setw (sizeof ("files")) << std::left << files
			<< " | "
			<< std::setw (sizeof ("name_size")) << std::left << name_size
			<< '\n' << '\n';

		file.seekg (0x20, std::ios::beg);

		class header
		{
		public:
			short n_size{ -1 };
			long size{ -1 };
			long z_size{ -1 };
			long offset{ -1 };
		};

		std::vector <header> headers{ static_cast <size_t> (files) };
		for (auto& header : headers) {
			file.read (reinterpret_cast <char*> (&header.n_size), sizeof (header.n_size));
			file.read (reinterpret_cast <char*> (&header.size), sizeof (header.size));
			file.read (reinterpret_cast <char*> (&header.z_size), sizeof (header.z_size));
			file.read (reinterpret_cast <char*> (&header.offset), sizeof (header.offset));
		}

		std::cout
			<< std::setw (9 + (std::numeric_limits <long>::digits * 3) + sizeof ("n_size")) << std::setfill ('-')
			<< '-'
			<< '\n'
			<< std::setfill (' ')
			<< std::setw (sizeof ("n_size")) << std::left << "n_size"
			<< " | "
			<< std::setw (std::numeric_limits <long>::digits) << std::left << "file_size"
			<< " | "
			<< std::setw (std::numeric_limits <long>::digits) << std::left << "z_size"
			<< " | "
			<< std::setw (std::numeric_limits <long>::digits) << std::left << "offset"
			<< '\n';

		for (const auto& header : headers) {
			std::cout
				<< std::setw (9 + (std::numeric_limits <long>::digits * 3) + sizeof ("n_size")) << std::setfill ('-')
				<< '-'
				<< '\n'
				<< std::setfill (' ')
				<< std::setw (sizeof ("n_size")) << std::left << header.n_size
				<< " | "
				<< std::setw (std::numeric_limits <long>::digits) << std::left << header.size
				<< " | "
				<< std::setw (std::numeric_limits <long>::digits) << std::left << header.z_size
				<< " | "
				<< std::setw (std::numeric_limits <long>::digits) << std::left << header.offset << '\n';
		}

		std::cout << '\n';

		long file_name_offset = files;
		file_name_offset *= 14;
		file_name_offset += 32;

		for (const auto& header : headers) {
			file.seekg (file_name_offset, std::ios::beg);

			file_name_offset += header.n_size;
			file_name_offset += 1;

			std::string name (header.n_size, '\0');
			file.read (std::data (name), header.n_size);

			std::cout << name << '\n';
			
			if (header.size != header.z_size) {
				std::cout << " (no support)";
			} else {				
				if (".fsb" != std::filesystem::path{ name }.extension ()) { continue; }

				file.seekg (header.offset, std::ios::beg);

				std::vector <char> sound_data;
				sound_data.resize (header.size);
				file.read (std::data (sound_data), header.size * sizeof (char));

				auto dest_folder{ std::filesystem::path{ _dest / _path.filename ().stem () / name }.replace_extension () };
				if (
					false == std::filesystem::create_directories (dest_folder) &&
					false == std::filesystem::exists (dest_folder)
					) {
					return;
				}

				_sound.extract (sound_data, dest_folder);
			}
			std::cout << '\n';
		}
	}
};

int main (int argc, char* argv[])
{
	std::vector <std::filesystem::path> files;
	files.reserve (argc - 1);

	auto dest{ std::filesystem::current_path () };
	for (int q{ 1 }; q != argc; ++q) {
		if (0 == strcmp (argv[q], "-dest")) {
			int next_id{ ++q };
			if (next_id == argc) { return EXIT_FAILURE; }

			dest = std::filesystem::path{ argv[next_id] };

			if (
				false == std::filesystem::create_directories (dest) &&
				false == std::filesystem::exists (dest)
				) {
				return EXIT_FAILURE;
			}
			continue;
		}
		files.emplace_back (argv[q]);
	}

	sound sound;
	for (const auto& file : files) {
		std::cout << file << '\n';
		extract::arc (sound, file, dest);
		std::cout << '\n' << '\n';
	}
}