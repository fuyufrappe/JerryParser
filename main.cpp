#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>

#include <zlib.h>

/**
 * Talismanの種類
 */
enum class JerryType
{
  GREEN,
  BLUE,
  PURPLE,
  GOLDEN,
  UNKNOWN
};

/**
 * 購入データ
 */
struct TalismanPurchase
{
  JerryType type;
  // Recombobulated?
  bool recombobulated;
  // 購入コスト
  long long cost;
};

/**
 * 数値にカンマ付けする関数（千単位区切り）
 */
std::string FormatNumber(long long num)
{
  std::stringstream ss;
  ss.imbue(std::locale(""));
  ss << std::fixed << num;
  return ss.str();
}

/**
 * コインを "1.3m" のような形式にフォーマットする
 * 1,000,000,000以上は "1.3B", 1,000,000以上は "1.3M", 1,000以上は "1.3K, それ以下はそのままの数値を返す
 */
std::string FormatCoins(long long coins)
{
  if (coins >= 1000000000)
  {
    double coin_unit_billions = coins / 1000000000.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << coin_unit_billions << "B";
    return ss.str();
  }
  else if (coins >= 1000000)
  {
    double coin_unit_millions = coins / 1000000.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << coin_unit_millions << "M";
    return ss.str();
  }
  else if (coins >= 1000)
  {
    double coin_unit_thousands = coins / 1000.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << coin_unit_thousands << "K";
    return ss.str();
  }
  else
  {
    return std::to_string(coins);
  }
}

/**
 * .log.gzファイルを読み込む関数
 */
std::string ReadGzFile(const std::string &filePath)
{
  gzFile file = gzopen(filePath.c_str(), "rb");
  if (!file)
  {
    std::cerr << "could not open a .gz file: " << filePath << std::endl;
    return "";
  }

  std::string content;
  char buffer[4096];
  int readBytes;

  while ((readBytes = gzread(file, buffer, sizeof(buffer))) > 0)
  {
    content.append(buffer, readBytes);
  }

  gzclose(file);
  return content;
}

/**
 * .logファイルを読み込む関数
 */
std::string ReadLogFile(const std::string &filePath)
{
  std::ifstream file(filePath);
  if (!file)
  {
    std::cerr << "could not open a .log file: " << filePath << std::endl;
    return "";
  }

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return content;
}

/**
 * GZip圧縮かどうかをマジックナンバーで判定する関数
 */
bool IsGzCompressed(const std::string &filePath)
{
  std::ifstream file(filePath, std::ios::binary);
  if (!file)
  {
    return false;
  }

  unsigned char header[2];
  file.read(reinterpret_cast<char *>(header), 2);

  // GZIPのマジックナンバー: {0x1F, 0x8B}
  return (file.gcount() == 2 && header[0] == 0x1F && header[1] == 0x8B);
}

/**
 * ファイルを読み込む関数
 */
std::string ReadFile(const std::string &filePath)
{
  if (IsGzCompressed(filePath))
  {
    return ReadGzFile(filePath);
  }
  else
  {
    return ReadLogFile(filePath);
  }
}

/**
 * Jerry Talismanの購入ログを抽出する関数
 */
std::vector<TalismanPurchase> ExtractJerryPurchases(const std::string &logContent)
{
  std::vector<TalismanPurchase> purchases;

  // Chat history regex
  std::regex logPattarn(
      R"(You purchased .+(.)(Green|Blue|PurPle|Golden) Jerry (Talisman|Artifact) .+for .+?([0-9,]+) coins)");

  std::sregex_iterator it(logContent.begin(), logContent.end(), logPattarn);
  std::sregex_iterator end;

  for (; it != end; ++it)
  {
    std::smatch match = *it;
    if (match.size() >= 5)
    {
      std::string rarity = match[1].str();
      std::string color = match[2].str();
      std::string costStr = match[4].str();

      // コストのカンマを除去して数値化
      costStr.erase(std::remove(costStr.begin(), costStr.end(), ','), costStr.end());
      costStr.erase(0, 1);
      long long cost;
      try
      {
        cost = std::stoll(costStr);
      }
      catch (const std::exception &e)
      {
        std::cerr << "Error processing row " << costStr << ": " << e.what() << std::endl;
        cost = 0;
      }

      // タリスマンの種類判定
      JerryType type = JerryType::UNKNOWN;
      if (color == "Green")
        type = JerryType::GREEN;
      else if (color == "Blue")
        type = JerryType::BLUE;
      else if (color == "Purple")
        type = JerryType::PURPLE;
      else if (color == "Golden")
        type = JerryType::GOLDEN;

      // Recombobulatedしてるかを判定
      bool recombobulated = (type == JerryType::GREEN && rarity == "9") ||  // 9=RARE
                            (type == JerryType::BLUE && rarity == "5") ||   // 5=EPIC
                            (type == JerryType::PURPLE && rarity == "6") || // 6=LEGENDARY
                            (type == JerryType::GOLDEN && rarity == "d");   // d=MYTHIC

      purchases.push_back({type, recombobulated, cost});
    }
  }

  return purchases;
}

int main()
{
  OPENFILENAMEA ofn;
  char fileNames[8192] = {0};

  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = NULL;
  ofn.lpstrFile = fileNames;
  ofn.nMaxFile = sizeof(fileNames);
  ofn.lpstrFilter = "Log Files (*.log;*.log.gz)\0*.log;*.log.gz\0All Files (*.*)\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER;

  std::vector<std::string> selectedFiles;

  if (GetOpenFileNameA(&ofn))
  {
    char *p = fileNames;
    std::string directory = p;
    p += directory.size() + 1;

    if (*p)
    { // 複数ファイル
      while (*p)
      {
        std::string filePath = directory + "\\" + p;
        selectedFiles.push_back(filePath);
        p += strlen(p) + 1;
      }
    }
    else
    { // 単一ファイル
      selectedFiles.push_back(directory);
    }
  }
  else
  {
    std::cerr << "No file chosen." << std::endl;
    return 1;
  }

  if (selectedFiles.empty())
  {
    std::cerr << "No file chosen." << std::endl;
    return 1;
  }

  // Recombobulatorの価格をユーザーに入力してもらう
  long long recombobulatorPrice = 0;
  std::cout << "Send Recombobulator3000 price: ";
  std::string recomPriceStr;
  std::getline(std::cin, recomPriceStr);
  // カンマがあれば削除
  recomPriceStr.erase(std::remove(recomPriceStr.begin(), recomPriceStr.end(), ','), recomPriceStr.end());

  auto [ptr, ec] =
      std::from_chars(recomPriceStr.data(), recomPriceStr.data() + recomPriceStr.size(), recombobulatorPrice);
  if (ec == std::errc{})
  {
    std::cout << "\033[32m\033[1m●\033[0m Set Recombobulator3000 price -> " << recombobulatorPrice << std::endl;
  }
  else
  {
    std::cout << "\033[31m\033[1m●\033[0m Invalid Input! ";
    std::cout << "autoset Recombobulator3000 price to 0" << std::endl;
  }

  // すべてのファイルからJerry Talisman購入情報を抽出
  std::vector<TalismanPurchase> allPurchases;

  for (const auto &filePath : selectedFiles)
  {
    std::cout << "Processing: " << filePath << std::endl;
    try
    {

      std::string content = ReadFile(filePath);
      if (!content.empty())
      {
        auto purchases = ExtractJerryPurchases(content);
        allPurchases.insert(allPurchases.end(), purchases.begin(), purchases.end());
      }
    }
    catch (const std::exception &e)
    {
      std::cerr << "Error processing file " << filePath << ": " << e.what() << std::endl;
      continue;
    }
  }

  // 集計処理
  long long totalCost = 0;
  int greenCount = 0;
  int recomGreenCount = 0;
  int blueCount = 0;
  int recomBlueCount = 0;
  int purpleCount = 0;
  int recomPurpleCount = 0;
  int goldenCount = 0;
  int recomGoldenCount = 0;

  for (const auto &purchase : allPurchases)
  {
    totalCost += purchase.cost;

    switch (purchase.type)
    {
    case JerryType::GREEN:
      if (purchase.recombobulated)
        recomGreenCount++;
      else
        greenCount++;
      break;
    case JerryType::BLUE:
      if (purchase.recombobulated)
        recomBlueCount++;
      else
        blueCount++;
      break;
    case JerryType::PURPLE:
      if (purchase.recombobulated)
        recomPurpleCount++;
      else
        purpleCount++;
      break;
    case JerryType::GOLDEN:
      if (purchase.recombobulated)
        recomGoldenCount++;
      else
        goldenCount++;
      break;
    default:
      break;
    }
  }

  // 上位のJerry TalismanをGreen Jerry Talismanに変換する際の処理
  int totalGreenEquivalent = greenCount + recomGreenCount;

  totalGreenEquivalent += blueCount * 5;
  totalGreenEquivalent += recomBlueCount * 5;

  totalGreenEquivalent += purpleCount * 25;
  totalGreenEquivalent += recomPurpleCount * 25;

  totalGreenEquivalent += goldenCount * 125;
  totalGreenEquivalent += recomGoldenCount * 125;

  // Recombobulatorの価格を調整
  long long adjustedCost =
      totalCost - (recomGreenCount + recomBlueCount + recomPurpleCount + recomGoldenCount) * recombobulatorPrice;

  // 平均価格の算出（Green Jerry Talisman換算）
  long long avgPricePerGreen = 0;
  if (totalGreenEquivalent > 0)
  {
    avgPricePerGreen = static_cast<long long>(static_cast<double>(adjustedCost) / totalGreenEquivalent);
  }

  // 結果表示
  std::cout << "\n=========== Jerry Talisman Parser ===========" << std::endl;
  std::cout << "All: " << FormatCoins(totalCost) << " (" << FormatNumber(totalCost) << " coins)" << std::endl;
  std::cout << "-------------------------------------------" << std::endl;
  std::cout << "Green: " << greenCount << std::endl;
  std::cout << "Recombobulated Green: " << recomGreenCount << std::endl;

  if (blueCount > 0 || recomBlueCount > 0)
  {
    std::cout << "Blue: " << blueCount << std::endl;
    std::cout << "Recombobulated Blue: " << recomBlueCount << std::endl;
  }

  if (purpleCount > 0 || recomPurpleCount > 0)
  {
    std::cout << "Purple: " << purpleCount << std::endl;
    std::cout << "Recombobulated Purple: " << recomPurpleCount << std::endl;
  }

  if (goldenCount > 0 || recomGoldenCount > 0)
  {
    std::cout << "Golden: " << goldenCount << std::endl;
    std::cout << "Recombobulated Golden: " << recomGoldenCount << std::endl;
  }

  std::cout << "-------------------------------------------" << std::endl;
  std::cout << "Green Jerry Talisman Conversion: " << totalGreenEquivalent << std::endl;
  std::cout << "Total Price Without Recombobulator: " << FormatCoins(adjustedCost) << " (" << FormatNumber(adjustedCost)
            << " coins)" << std::endl;
  std::cout << "Per Green Jerry Talisman: " << FormatCoins(avgPricePerGreen) << " (" << FormatNumber(avgPricePerGreen)
            << " coins)" << std::endl;
  std::cout << "=============================================" << std::endl;

  std::cout << "\nPress Enter to exit...";
  std::cin.get();

  return 0;
}
