#include <cerrno>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <grp.h>
#include <iomanip>
#include <iostream>
#include <pwd.h>
#include <sys/stat.h>

char get_filetype_character(std::filesystem::directory_entry &entry) {
  switch (entry.symlink_status().type()) {
  case std::filesystem::file_type::regular:
    return '-';
  case std::filesystem::file_type::directory:
    return 'd';
  case std::filesystem::file_type::block:
    return 'b';
  case std::filesystem::file_type::character:
    return 'c';
  default:
    return 'o';
  }
}

std::string get_perms(std::filesystem::perms &perms) {
  std::string perms_str{};

  auto append = [&](char op, std::filesystem::perms perm) {
    perms_str += (std::filesystem::perms::none == (perm & perms)) ? '-' : op;
  };

  // FIXME: setgid, setuid and sticky bits?
  append('r', std::filesystem::perms::owner_read);
  append('w', std::filesystem::perms::owner_write);
  append('x', std::filesystem::perms::owner_exec);
  append('r', std::filesystem::perms::group_read);
  append('w', std::filesystem::perms::group_write);
  append('x', std::filesystem::perms::group_exec);
  append('r', std::filesystem::perms::others_read);
  append('w', std::filesystem::perms::others_write);
  append('x', std::filesystem::perms::others_exec);

  return perms_str;
}

int main(int argc, char *argv[]) {
  auto path = ".";
  if (argc > 1) {
    path = argv[1];
  }

  bool is_dir = std::filesystem::is_directory(path);
  if (!is_dir) {
    std::cout << "path is not a directory";
  }

  std::filesystem::directory_iterator it{std::filesystem::path{path}};

  for (auto x : it) {
    std::cout << get_filetype_character(x);
    auto permissions = x.symlink_status().permissions();
    std::cout << get_perms(permissions);
    std::cout << " " << x.hard_link_count() << " ";

    // looks like we need to use c methods to get the username & password - why?
    struct stat info;
    auto err = lstat(x.path().c_str(), &info);
    if (err != 0) {
      std::cout << "wtf" << errno;
    }
    auto pw = getpwuid(info.st_uid);
    auto gr = getgrgid(info.st_gid);

    if (pw != nullptr) {
      std::cout << pw->pw_name;
    } else {
      std::cout << info.st_uid;
    }
    std::cout << " \t";
    if (gr != nullptr) {
      std::cout << gr->gr_name;
    } else {
      std::cout << info.st_gid;
    }
    std::cout << " \t";
    std::cout << info.st_size;
    std::cout << "\t";
    auto tp = std::chrono::system_clock::time_point{
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::seconds{info.st_mtim.tv_sec} +
            std::chrono::nanoseconds{info.st_mtim.tv_nsec})};

    auto a = std::chrono::system_clock::to_time_t(tp);
    auto b = std::put_time(std::localtime(&a), "%Y-%m-%d %X");
    std::cout << b;

    std::cout << " ";

    std::cout << x.path().stem().string();

    std::cout << "\n";
  }
}
