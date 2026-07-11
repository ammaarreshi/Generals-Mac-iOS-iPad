/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "PreRTS.h"

#include "Common/SagePatchIni.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace
{
struct GameDataBlock
{
	GameDataBlock() :
		endLineStart(std::string::npos),
		hasFpsLimit(false),
		hasUseFpsLimit(false),
		indentation("  "),
		indentationSet(false)
	{
	}

	std::string::size_type endLineStart;
	bool hasFpsLimit;
	bool hasUseFpsLimit;
	std::string indentation;
	bool indentationSet;
};

bool isHorizontalWhitespace(char character)
{
	return character == ' ' || character == '\t';
}

std::string trim(const std::string &value)
{
	std::string::size_type begin = 0;
	while (begin < value.size() && isHorizontalWhitespace(value[begin]))
	{
		++begin;
	}

	std::string::size_type end = value.size();
	while (end > begin && isHorizontalWhitespace(value[end - 1]))
	{
		--end;
	}

	return value.substr(begin, end - begin);
}

char toLowerAscii(char character)
{
	if (character >= 'A' && character <= 'Z')
	{
		return character + ('a' - 'A');
	}
	return character;
}

bool equalsIgnoreCase(const std::string &lhs, const char *rhs)
{
	const std::string::size_type rhsLength = std::strlen(rhs);
	if (lhs.size() != rhsLength)
	{
		return false;
	}

	for (std::string::size_type i = 0; i < lhs.size(); ++i)
	{
		if (toLowerAscii(lhs[i]) != toLowerAscii(rhs[i]))
		{
			return false;
		}
	}
	return true;
}

std::string removeIniComment(const std::string &line)
{
	const std::string::size_type commentStart = line.find(';');
	return commentStart == std::string::npos ? line : line.substr(0, commentStart);
}

std::string removeUtf8Bom(const std::string &line, std::string::size_type lineStart)
{
	if (lineStart == 0 && line.size() >= 3
		&& static_cast<unsigned char>(line[0]) == 0xEF
		&& static_cast<unsigned char>(line[1]) == 0xBB
		&& static_cast<unsigned char>(line[2]) == 0xBF)
	{
		return line.substr(3);
	}
	return line;
}

std::string::size_type nextLineStart(const std::string &contents, std::string::size_type lineEnd)
{
	if (lineEnd == contents.size())
	{
		return lineEnd;
	}
	if (contents[lineEnd] == '\r' && lineEnd + 1 < contents.size() && contents[lineEnd + 1] == '\n')
	{
		return lineEnd + 2;
	}
	return lineEnd + 1;
}

bool findGameDataBlock(const std::string &contents, GameDataBlock &block)
{
	bool inGameData = false;
	std::string::size_type lineStart = 0;

	while (lineStart < contents.size())
	{
		std::string::size_type lineEnd = contents.find_first_of("\r\n", lineStart);
		if (lineEnd == std::string::npos)
		{
			lineEnd = contents.size();
		}

		std::string line = contents.substr(lineStart, lineEnd - lineStart);
		line = removeUtf8Bom(line, lineStart);
		const std::string statement = trim(removeIniComment(line));

		if (!inGameData)
		{
			inGameData = equalsIgnoreCase(statement, "GameData");
		}
		else if (equalsIgnoreCase(statement, "End"))
		{
			block.endLineStart = lineStart;
			return true;
		}
		else
		{
			const std::string::size_type equals = statement.find('=');
			if (equals != std::string::npos)
			{
				const std::string key = trim(statement.substr(0, equals));
				block.hasFpsLimit = block.hasFpsLimit || equalsIgnoreCase(key, "FramesPerSecondLimit");
				block.hasUseFpsLimit = block.hasUseFpsLimit || equalsIgnoreCase(key, "UseFPSLimit");

				if (!block.indentationSet)
				{
					const std::string::size_type firstCharacter = line.find_first_not_of(" \t");
					if (firstCharacter != std::string::npos)
					{
						block.indentation = line.substr(0, firstCharacter);
						block.indentationSet = true;
					}
				}
			}
		}

		lineStart = nextLineStart(contents, lineEnd);
	}

	return false;
}

std::string detectLineEnding(const std::string &contents, std::string::size_type lineStart)
{
	if (lineStart > 0)
	{
		const std::string::size_type lineEnd = lineStart - 1;
		if (contents[lineEnd] == '\n' && lineEnd > 0 && contents[lineEnd - 1] == '\r')
		{
			return "\r\n";
		}
		if (contents[lineEnd] == '\r' || contents[lineEnd] == '\n')
		{
			return contents.substr(lineEnd, 1);
		}
	}

	const std::string::size_type lineEnd = contents.find_first_of("\r\n");
	if (lineEnd == std::string::npos)
	{
		return "\n";
	}
	if (contents[lineEnd] == '\r' && lineEnd + 1 < contents.size() && contents[lineEnd + 1] == '\n')
	{
		return "\r\n";
	}
	return contents.substr(lineEnd, 1);
}

bool readCompleteFile(const char *path, std::string &contents)
{
	FILE *file = std::fopen(path, "rb");
	if (file == nullptr)
	{
		return false;
	}

	bool succeeded = std::fseek(file, 0, SEEK_END) == 0;
	const long fileSize = succeeded ? std::ftell(file) : -1;
	succeeded = succeeded && fileSize >= 0 && std::fseek(file, 0, SEEK_SET) == 0;

	if (succeeded)
	{
		contents.resize(static_cast<std::string::size_type>(fileSize));
		if (fileSize > 0)
		{
			succeeded = std::fread(&contents[0], 1, static_cast<std::size_t>(fileSize), file)
				== static_cast<std::size_t>(fileSize);
		}
	}

	if (std::fclose(file) != 0)
	{
		succeeded = false;
	}
	return succeeded;
}

bool replaceFileAtomically(const char *path, const std::string &contents)
{
	const std::string temporaryPath = std::string(path) + ".generalsx.tmp";
	FILE *file = std::fopen(temporaryPath.c_str(), "wb");
	if (file == nullptr)
	{
		return false;
	}

	bool succeeded = contents.empty()
		|| std::fwrite(contents.data(), 1, contents.size(), file) == contents.size();
	if (succeeded)
	{
		succeeded = std::fflush(file) == 0;
	}
	if (std::fclose(file) != 0)
	{
		succeeded = false;
	}

	if (!succeeded || std::rename(temporaryPath.c_str(), path) != 0)
	{
		std::remove(temporaryPath.c_str());
		return false;
	}
	return true;
}
} // namespace

namespace SagePatchIni
{
// GeneralsX @bugfix Codex 10/07/2026 Preserve complete SagePatch.ini contents during FPS-default migration.
MigrationResult migrateFpsDefaults(const char *path)
{
	if (path == nullptr || path[0] == '\0')
	{
		return MIGRATION_FAILED;
	}

	std::string contents;
	if (!readCompleteFile(path, contents))
	{
		return MIGRATION_FAILED;
	}

	GameDataBlock block;
	if (!findGameDataBlock(contents, block) || block.hasFpsLimit)
	{
		return MIGRATION_UNCHANGED;
	}

	const std::string lineEnding = detectLineEnding(contents, block.endLineStart);
	std::string migration;
	migration += block.indentation + "; Migrated 60 FPS defaults" + lineEnding;
	if (!block.hasUseFpsLimit)
	{
		migration += block.indentation + "UseFPSLimit = Yes" + lineEnding;
	}
	migration += block.indentation + "FramesPerSecondLimit = 60" + lineEnding;

	const std::string migratedContents = contents.substr(0, block.endLineStart)
		+ migration + contents.substr(block.endLineStart);
	return replaceFileAtomically(path, migratedContents) ? MIGRATION_UPDATED : MIGRATION_FAILED;
}

} // namespace SagePatchIni
