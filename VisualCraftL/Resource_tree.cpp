/*
 Copyright © 2021-2023  TokiNoBug
This file is part of SlopeCraft.

    SlopeCraft is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SlopeCraft is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SlopeCraft. If not, see <https://www.gnu.org/licenses/>.

    Contact with me:
    github:https://github.com/SlopeCraft/SlopeCraft
    bilibili:https://space.bilibili.com/351429231
*/

#include "Resource_tree.h"

#include <zip.h>

#include <filesystem>

#include "VCL_internal.h"

// #include "VisualCraftL.h"

auto split_by_slash(std::string_view str) noexcept {
  std::vector<std::basic_string_view<char>> result;

  int64_t substr_start = 0;

  for (int64_t cur_slash = 0; cur_slash < int64_t(str.length()); cur_slash++) {
    if (str.at(cur_slash) == '/') {
      if (cur_slash != 0) {
        result.emplace_back(str.substr(substr_start, cur_slash - substr_start));
      }
      substr_start = cur_slash += 1;
    }
  }
  // if (str.back() != '/')
  result.emplace_back(str.substr(substr_start));

  return result;
}

zipped_folder zipped_folder::from_zip(std::string_view zipname,
                                      bool *const ok) noexcept {
  zipped_folder result;
  if (true) {
    std::filesystem::path path = (const char8_t *)(zipname).data();
    if (zipname.empty()) {
      if (ok)
        *ok = false;
      std::string msg =
          fmt::format("The filename \"{}\" of zip is empty.", zipname);
      ::VCL_report(VCL_report_type_t::error, msg.c_str());
      return result;
    }

    if (!std::filesystem::is_regular_file(path)) {
      if (ok)
        *ok = false;
      std::string msg = fmt::format(
          "The filename \"{}\" does not refer to a regular file.", zipname);
      ::VCL_report(VCL_report_type_t::error, msg.c_str());
      return result;
    }

    if (path.extension() != ".zip") {
      if (ok)
        *ok = false;
      std::string msg = fmt::format(
          "The filename \"{}\" extension name is not .zip", zipname);
      ::VCL_report(VCL_report_type_t::error, msg.c_str());
      return result;
    }
  }
  int errorcode;
  zip_t *const zip = zip_open(zipname.data(), ZIP_RDONLY, &errorcode);

  if (zip == NULL) {
    if (ok)
      *ok = false;
    std::string msg = fmt::format(
        "Failed to open zip file : {}, error code = {}", zipname, errorcode);
    ::VCL_report(VCL_report_type_t::error, msg.c_str());
    return result;
  }

  const int64_t entry_num = zip_get_num_entries(zip, ZIP_FL_UNCHANGED);

  for (int64_t entry_idx = 0; entry_idx < entry_num; entry_idx++) {
    auto splited =
        split_by_slash(::zip_get_name(zip, entry_idx, ZIP_FL_ENC_GUESS));

    zipped_folder *curfolder = &result;
    zipped_file *destfile = nullptr;
    for (size_t idx = 0; idx + 1 < splited.size(); idx++) {
      if (idx + 1 < splited.size()) {
        // is folder name
        curfolder = &curfolder->subfolders[std::string(splited.at(idx))];
      }
    }

    if (std::string_view(::zip_get_name(zip, entry_idx, ZIP_FL_ENC_GUESS))
            .back() != '/') {
      auto ret = curfolder->files.emplace(splited.back(), zipped_file());
      destfile = &ret.first->second;
    }

    if (destfile == nullptr) {
      continue;
    }

    zip_stat_t stat;
    zip_stat_index(zip, entry_idx, ZIP_FL_UNCHANGED, &stat);

    destfile->__data.resize(stat.size);
    zip_file_t *const zfile = zip_fopen_index(zip, entry_idx, ZIP_FL_UNCHANGED);
    if (zfile == NULL) {
      if (ok)
        *ok = false;
      std::string msg = fmt::format(
          "Failed to open file in zip. index : {}, file name : {}\n", entry_idx,
          ::zip_get_name(zip, entry_idx, ZIP_FL_ENC_GUESS));
      ::VCL_report(VCL_report_type_t::error, msg.c_str());
      continue;
    }

    zip_fread(zfile, destfile->__data.data(), stat.size);
  }

  if (ok)
    *ok = true;

  return result;
}

void zipped_folder::merge_from_base(const zipped_folder &source_base) noexcept {
  for (const auto &it : source_base.files) {
    auto find = this->files.find(it.first);

    if (find == this->files.end()) {
      this->files.emplace(it.first, it.second);
    } else {
      // find->second = it.second;
    }
  }

  for (const auto &it : source_base.subfolders) {
    auto find = this->subfolders.find(it.first);

    if (find == this->subfolders.end()) {
      this->subfolders.emplace(it.first, it.second);
    } else {
      find->second.merge_from_base(it.second);
    }
  }
}

void zipped_folder::merge_from_base(zipped_folder &&source_base) noexcept {
  this->files.merge(std::move(source_base.files));

  for (auto &it : source_base.subfolders) {
    auto find = this->subfolders.find(it.first);

    if (find == this->subfolders.end()) {
      this->subfolders.emplace(it.first, std::move(it.second));
    } else {
      find->second.merge_from_base(std::move(it.second));
    }
  }
}