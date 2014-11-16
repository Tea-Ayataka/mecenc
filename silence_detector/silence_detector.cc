#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace std;

bool CompareCharacters(const char *expected, char **actual) {
  const size_t length = strlen(expected);
  bool result = true;
  for (size_t i = 0; i < length; ++i) {
    result &= expected[i] == (*actual)[i];
  }

  /*
  char buf[length + 1];
  for (size_t i = 0; i < length; ++i) {
    buf[i] = (*actual)[i];
  }
  buf[length] = '\0';
  cerr << "expected: " << expected << ", actual: " << buf << endl;
  */
  *actual += length;
  return result;
}

string ReadString(size_t size, char **buf) {
  string str(*buf, 0, size);
  *buf += size;
  return str;
}

int ReadNumber(char **buf, size_t size) {
  int result;
  unsigned int mask;
  switch (size) {
  case 2:
    result = static_cast<int>(*reinterpret_cast<short *>(*buf));
    break;
  case 3:
//    result = (*buf)[0] | ((*buf)[1] << 8) | ((*buf)[2] << 16) | (-(((*buf)[2] & 0x80) << 17));
    mask = ((*buf)[2] & 0x80) ? 0xFF000000 : 0x0;
    result = static_cast<int>((*reinterpret_cast<unsigned int *>(*buf) & 0x00FFFFFF) | mask);
//    result = static_cast<int>((*reinterpret_cast<unsigned int *>(*buf) & 0x00FFFFFF) | (-(((*buf)[2] & 0x80) << 17)));
    break;
  case 4:
    result = *reinterpret_cast<int *>(*buf);
    break;
  }
  *buf += size;
  return result;  
}

short ReadShort(char **buf) {
  const short result = *reinterpret_cast<short *>(*buf);
  *buf += 2;
  return result;
}

unsigned int ReadUnsignedInt(char **buf) {
  const unsigned int result = *reinterpret_cast<int *>(*buf);
  *buf += 4;
  return result;
}

unsigned short ReadUnsignedShort(char **buf) {
  const unsigned short result = *reinterpret_cast<unsigned short *>(*buf);
  *buf += 2;
  return result;
}

string SamplingToTime(size_t sampling_counter, size_t sampling_rate) {
  stringstream ss;
  const int milli_seconds = 1000 * sampling_counter / sampling_rate;
  const int seconds = milli_seconds / 1000;
  ss << seconds << "." << setfill('0') << setw(3) << (milli_seconds % 1000);
  /*
  const int minutes = seconds / 60;
  const int hours = minutes / 60;
  ss << setfill('0') << setw(2) << hours
     << ":"
     << setfill('0') << setw(2) << minutes % 60
     << ":"
     << setfill('0') << setw(2) << seconds % 60
     << "."
     << setfill('0') << setw(3) << milli_seconds % 1000;
  */
  return ss.str();
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    cerr << "Usage: " << argv[0] << " filename" << endl;
    return -1;
  }

  ifstream ifs(argv[1]);
  if (!ifs) {
    cerr << "Failed to open file: " << argv[1] << endl;
    return -1;
  }
  ifs.seekg(0, std::ios::end);
  const size_t file_size = static_cast<size_t>(ifs.tellg());
  ifs.seekg(0, std::ios::beg);

  char *file_buf_head = new char[file_size + 1];  // + 1 is for 24bit PCM (ReadNumber)
  char *file_buf_ptr = file_buf_head;
  if (!ifs.read(file_buf_head, file_size)) {
    cerr << "Failed to read the file." << endl;
    return -1;
  }

  if (!CompareCharacters("RIFF", &file_buf_ptr)) {
    cerr << "Invalid RIFF format." << endl;
    return -1;
  }

  if (ReadUnsignedInt(&file_buf_ptr) + 8 != file_size) {
    cerr << "Invalid file size." << endl;
    cerr << "pos: " << (file_buf_ptr - file_buf_head) << endl;
    return -1;
  }

  if (!CompareCharacters("WAVEfmt ", &file_buf_ptr)) {
    cerr << "Invalid WAVE format." << endl;
    cerr << "pos: " << (file_buf_ptr - file_buf_head) << endl;
    return -1;
  }

  const size_t fmt_size = ReadUnsignedInt(&file_buf_ptr);
  const unsigned short format_id = ReadUnsignedShort(&file_buf_ptr);
  // ffmpeg output invalid format_id for 24bit or 32bit PCM.
  //  if (format_id != 1) {
  //    cerr << "Invalid format ID: " << format_id << endl;
  //    return -1;
  //  }

  const int channel_num = ReadUnsignedShort(&file_buf_ptr);
  const int sampling_rate = ReadUnsignedInt(&file_buf_ptr);
  const int sampling_bytes =
    ReadUnsignedInt(&file_buf_ptr) / channel_num / sampling_rate;
  if (sampling_bytes < 2 || 4 < sampling_bytes) {
    cerr << "Unsupported sampling bytes: " << sampling_bytes << endl;
    return -1;
  }
  if (ReadUnsignedShort(&file_buf_ptr) != channel_num * sampling_bytes) {
    cerr << "Invalid block size." << endl;
    return -1;
  }
  if (ReadUnsignedShort(&file_buf_ptr) != sampling_bytes * 8) {
    cerr << "Invalid sampling bits." << endl;
    return -1;
  }

  const size_t extend_header_size = ReadUnsignedShort(&file_buf_ptr);
  file_buf_ptr += extend_header_size;

  while (true) {
    if (file_buf_ptr - file_buf_head > 1024 - 4) {
      cerr << "Too big header." << endl;
      return -1;
    }
    if (CompareCharacters("data", &file_buf_ptr)) {
      break;
    }
    size_t chunk_size = ReadUnsignedInt(&file_buf_ptr);
    file_buf_ptr += chunk_size;
  }

  const size_t data_size = ReadUnsignedInt(&file_buf_ptr);
  if (data_size + (file_buf_ptr - file_buf_head) != file_size) {
    cerr << "Invalid data size." << endl;
    return -1;
  }

  vector<pair<size_t, size_t> > mute_ranges;
  const int kVolumeThreshold = 12 * (1 << ((sampling_bytes - 2) * 8));
  const int kVolumeDiffThreshold = 2;
  const int minimum_mute_chunk_threshold = sampling_rate / 100;  // 10msec
  int mute_counter = 0;
  const size_t max_sampling_count = data_size / (sampling_bytes * channel_num);

  for (size_t sampling_counter = 0; sampling_counter < max_sampling_count;
       ++sampling_counter) {
    const int left_sound = ReadNumber(&file_buf_ptr, sampling_bytes);
    const int right_sound = ReadNumber(&file_buf_ptr, sampling_bytes);

    // TODO: It may be better to use the differences from the previous values.
    if (abs(left_sound) <= kVolumeThreshold && abs(right_sound) <= kVolumeThreshold) { 
      ++mute_counter;
      if (mute_counter == minimum_mute_chunk_threshold) {
        mute_ranges.push_back(make_pair(sampling_counter - minimum_mute_chunk_threshold + 1, 0));
      }
    } else {
      if (mute_counter >= minimum_mute_chunk_threshold) {
        mute_counter = 0;
        mute_ranges.back().second = sampling_counter - 1;
      }
    }
  }

  const size_t concat_threshold = sampling_rate / 1000;  // 1msec
  for (size_t i = 1; i < mute_ranges.size(); ++i) {
    if (mute_ranges[i].first - mute_ranges[i - 1].second < concat_threshold) {
      mute_ranges[i - 1].second = mute_ranges[i].second;
      mute_ranges.erase(mute_ranges.begin() + i);
      --i;
    }
  }
  const size_t mute_chunk_threshold = sampling_rate * 0.3;  // 300msec
  for (int i = 0; i < static_cast<int>(mute_ranges.size()); ++i) {
    if (mute_ranges[i].second - mute_ranges[i].first < mute_chunk_threshold) {
      mute_ranges.erase(mute_ranges.begin() + i);
      --i;
    }
  }

  cout << "all 0.000 " << SamplingToTime(max_sampling_count, sampling_rate) << endl;
  for (size_t i = 0; i < mute_ranges.size(); ++i) {
    const pair<size_t, size_t> &range = mute_ranges[i];
    cout << SamplingToTime(range.first, sampling_rate) << " "
         << SamplingToTime(range.second, sampling_rate) << endl;
    /*
    const float width =
      static_cast<float>((range.second - range.first)) / sampling_rate;
    cout << SamplingToTime(range.first, sampling_rate) << "\t"
         << SamplingToTime(range.second, sampling_rate) << "\t("
         << width << ")" << endl;
    */
  }

  delete [] file_buf_head;

  return 0;
}
