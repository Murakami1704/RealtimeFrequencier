# include <Siv3D.hpp>

// 非線形ロジスティック回帰結果
namespace ClapModel {
	// --- 単体項 ---
	const double M_CrestFactor = 41.1050;
	const double S_CrestFactor = 26.0275;
	const double W_CrestFactor = 0.9338;

	// --- 二乗項 (x^2) ---
	const double M_RMS2 = 0.0001;
	const double S_RMS2 = 0.0001;
	const double W_RMS2 = -1.4475;

	const double M_Centroid2 = 69862658.4538;
	const double S_Centroid2 = 11359442.2800;
	const double W_Centroid2 = -1.3833;

	// --- 相互作用項 (x * y) ---
	const double M_RMSxCrest = 0.3160;
	const double S_RMSxCrest = 0.2182;
	const double W_RMSxCrest = 2.0790;

	const double M_ZCRxCrest = 3.8130;
	const double S_ZCRxCrest = 2.2004;
	const double W_ZCRxCrest = 0.9169;

	const double M_ZCRxCentroid = 793.4064;
	const double S_ZCRxCentroid = 97.0611;
	const double W_ZCRxCentroid = -0.0714;

	const double M_ZCRxFlux = 0.0511;
	const double S_ZCRxFlux = 0.1244;
	const double W_ZCRxFlux = 2.6366;

	// バイアス
	const double BIAS = -2.2485;
}

// 定数宣言
const int NORMAL_MODE = 1;
const int REMAIN_MODE = 2;
const int DELTA_MODE = 3;

const int REALTIME_SCENE = 1;
const int SOUNDPLAY_SCENE = 2;

// 変数宣言
int mode = NORMAL_MODE;
int sceneNum = REALTIME_SCENE;

// レコーディング用変数
Array<Image> frames;
bool isRecording = false;
Array<double> recordingTimeArray;
Array<double> powerArray;
Array<double> averageArray;
Array<double> flatnessArray;
Array<double> centroidArray;
Array<double> rolloffArray;
Array<double> fluxArray;

// 録画用配列の追加
Array<double> rmsArray;
Array<double> zcrArray;
Array<double> crestArray;

Array<int> analyzedArray;

Array<Array<double>> waveData;
Array<Array<double>> soundData;
double recordingTime = 0;

int recordingBeginIndent = 0;
int recordingEndIndent = 0;
int recordingSize = 0;

//画面サイズ及びバッファサイズの保存変数
double sceneWidth;
double sceneHeight;
int32 bufferSize;

// 高速フーリエ変換の定義
FFTResult fft;

// ピークを保持するための配列
Array<double> peakBuffer;

// ひとつ前を保持するための配列
Array<double> prevBuffer;

// 生データの振幅を保存する変数
Array<double> nowSoundBuffer;

// 100ms分の生波形を保持するバッファ
Array<double> waveBuffer;

// 周波数帯域ごとの平均値ととる
double frequencyAve = 0;
double frequencyLog = 0;

// 解析結果
double spectralFlatness = 0;
double spectralEnergy = 0;
double spectralCentroid = 0;
double spectralRolloff = 0;
double spectralFlux = 0;
double spectralPower = 0;

double spectralSum = 0;

// 時間領域の解析結果
double timeRMS = 0;
double timeZCR = 0;
double timeCrestFactor = 0;

bool analyzed = false;


// テキストボックス
TextEditState recordingBeginText;
TextEditState recordingEndText;

double score = 0;

double time_RF = 0;

bool predictClap() {

	score = 0;

	// 【ガード1】音量による物理足切り（環境に合わせて 0.010 〜 0.020 で微調整）
	if (timeRMS < 0.015) return false;

	score = ClapModel::BIAS;

	// 1. 単体項の加算 (CrestFactorのみ)
	score += ClapModel::W_CrestFactor * (timeCrestFactor - ClapModel::M_CrestFactor) / ClapModel::S_CrestFactor;

	// 2. 二乗項の加算
	double valRMS2 = (timeRMS * timeRMS);
	score += ClapModel::W_RMS2 * (valRMS2 - ClapModel::M_RMS2) / ClapModel::S_RMS2;

	double valCentroid2 = (spectralCentroid * spectralCentroid);
	score += ClapModel::W_Centroid2 * (valCentroid2 - ClapModel::M_Centroid2) / ClapModel::S_Centroid2;

	// 3. 相互作用項（掛け算）の加算
	double valRMSxCrest = (timeRMS * timeCrestFactor);
	score += ClapModel::W_RMSxCrest * (valRMSxCrest - ClapModel::M_RMSxCrest) / ClapModel::S_RMSxCrest;

	double valZCRxCrest = (timeZCR * timeCrestFactor);
	score += ClapModel::W_ZCRxCrest * (valZCRxCrest - ClapModel::M_ZCRxCrest) / ClapModel::S_ZCRxCrest;

	double valZCRxCentroid = (timeZCR * spectralCentroid);
	score += ClapModel::W_ZCRxCentroid * (valZCRxCentroid - ClapModel::M_ZCRxCentroid) / ClapModel::S_ZCRxCentroid;

	double valZCRxFlux = (timeZCR * spectralFlux);
	score += ClapModel::W_ZCRxFlux * (valZCRxFlux - ClapModel::M_ZCRxFlux) / ClapModel::S_ZCRxFlux;

	// スコア確認用デバッグ（必要に応じてコメントアウト解除）
	Print << U"Score: {:.3f}"_fmt(score);

	return (score > 0);
}

void buttonUI() {
	if (SimpleGUI::Button(U"Mode:Normal", Vec2{ 260, 20 }))
	{
		mode = NORMAL_MODE;
	}

	if (SimpleGUI::Button(U"Mode:Remain", Vec2{ 260, 70 }))
	{
		mode = REMAIN_MODE;
	}

	if (SimpleGUI::Button(U"Mode:Delta", Vec2{ 260, 120 }))
	{
		mode = DELTA_MODE;
	}
}

void textBoxUI() {
	SimpleGUI::TextBox(recordingBeginText, Vec2{ 10, 170 }, 150);
	SimpleGUI::TextBox(recordingEndText, Vec2{ 170, 170 }, 150);

	String beginText_TextBoxUI;
	String endText_TextBoxUI;

	if (SimpleGUI::Button(U"Recording num Input", Vec2{ 330, 170 }))
	{
		if (!isRecording)
		{
			beginText_TextBoxUI = recordingBeginText.text;
			endText_TextBoxUI = recordingEndText.text;

			double beginNum_TextBoxUI = -1;
			double endNum_TextBoxUI = -1;

			bool errorFlug = false;

			if (beginText_TextBoxUI == U"min")
			{
				recordingBeginIndent = 0;
			}
			else if (auto val = ParseOpt<double>(beginText_TextBoxUI))
			{
				beginNum_TextBoxUI = Parse<double>(beginText_TextBoxUI);
			}
			else
			{
				errorFlug = true;
			}

			if (endText_TextBoxUI == U"max")
			{
				recordingEndIndent = peakBuffer.size();
			}
			else if (auto val = ParseOpt<double>(endText_TextBoxUI))
			{
				endNum_TextBoxUI = Parse<double>(endText_TextBoxUI);
			}
			else
			{
				errorFlug = true;
			}

			if (beginNum_TextBoxUI >= 0 && endNum_TextBoxUI >= 0)
			{
				if (beginNum_TextBoxUI <= endNum_TextBoxUI) {
					recordingBeginIndent = beginNum_TextBoxUI / fft.resolution;
					recordingEndIndent = endNum_TextBoxUI / fft.resolution;
				}
				else
				{
					errorFlug = true;
				}
			}

			if (errorFlug)
			{
				recordingBeginIndent = 0;
				recordingEndIndent = peakBuffer.size();
				recordingSize = recordingEndIndent - recordingBeginIndent + 1;
				return;
			}

			recordingSize = recordingEndIndent - recordingBeginIndent + 1;
		}
	}

	double beginPos_TextBoxUI = 1.0 * recordingBeginIndent / peakBuffer.size() * sceneWidth;
	double endPos_TextBoxUI = 1.0 * recordingEndIndent / peakBuffer.size() * sceneWidth;

	RectF{ beginPos_TextBoxUI, 0, 1, sceneHeight }.draw(Palette::Red);
	RectF{ endPos_TextBoxUI - 1, 0, 1, sceneHeight }.draw(Palette::Red);
}

void softInit() {
	if (peakBuffer.size() != fft.buffer.size())
	{
		peakBuffer.resize(fft.buffer.size(), 0.0);
	}
	if (prevBuffer.size() != fft.buffer.size())
	{
		prevBuffer.resize(fft.buffer.size(), 0.0);
	}
}

void mouseUI() {
	double mouseXIdx = (Cursor::Pos().x / sceneWidth) * bufferSize;
	double frequency = mouseXIdx * fft.resolution;

	RectF{ Cursor::Pos().x, 0, 1, sceneHeight }.draw(Palette::White);

	Print << U"周波数: {:.1f} Hz (idx: {})"_fmt(frequency, (int32)mouseXIdx);
	if (frequency > 1000) {
		Print << U"({:.2f} kHz)"_fmt(frequency / 1000.0);
	}
	Print << U"平均音量{:.5f}"_fmt(frequencyAve);
	Print << U"フラットネス{:.3f}"_fmt(spectralFlatness);
	Print << U"エネルギー{:.5f}"_fmt(spectralEnergy);
}

void displayFrequency(double currentVal, int i) {
	// ピーク値の表示
	if (mode == NORMAL_MODE) {
		// 現在の値を表示
		peakBuffer[i] = currentVal;
	}
	else if (mode == REMAIN_MODE) {
		// 現在の値か、少し減衰させた過去のピーク値の大きい方
		peakBuffer[i] = Max(currentVal, peakBuffer[i] * 0.92);
	}
	else if (mode == DELTA_MODE) {
		peakBuffer[i] = currentVal - prevBuffer[i];
		if (peakBuffer[i] < 0) {
			peakBuffer[i] = 0;
		}
	}
	peakBuffer[i] += 1e-10;
	if (i >= recordingBeginIndent && i <= recordingEndIndent) {
		frequencyLog += Log(peakBuffer[i]);

		frequencyAve += peakBuffer[i];

		spectralEnergy += peakBuffer[i] * peakBuffer[i];

		spectralCentroid += fft.resolution * i * peakBuffer[i];
		spectralSum += peakBuffer[i];

		spectralFlux += (currentVal - prevBuffer[i]) * (currentVal - prevBuffer[i]);
		spectralPower += currentVal * currentVal;
	}
	double x = (double)i / bufferSize * sceneWidth;
	double h = peakBuffer[i] * 800; // 倍率を調整

	// 描画
	if (sceneNum == REALTIME_SCENE) {
		RectF{ Arg::bottomLeft(x, sceneHeight), 2, h }.draw(HSV{ 240 - i * 0.05, 0.8, 1.0 });
	}
	else if (sceneNum == SOUNDPLAY_SCENE) {
		RectF{ Arg::bottomLeft(x, sceneHeight - 120), 2, h }.draw(HSV{ 240 - i * 0.05, 0.8, 1.0 });
	}

	prevBuffer[i] = currentVal;
}

void timeDomainParaCalc(const Array<double>& samples) {
	if (samples.isEmpty()) return;

	double sumSq = 0;
	double maxAbs = 0;
	int zcrCount = 0;
	const int N = samples.size();

	for (int i = 0; i < N; ++i) {
		double s = samples[i];
		double sAbs = Abs(s);

		// 1. RMSのための二乗和
		sumSq += s * s;

		// 2. Crest Factorのための最大値
		if (sAbs > maxAbs) maxAbs = sAbs;

		// 3. ZCRのための符号反転チェック
		if (i > 0) {
			if ((samples[i] >= 0 && samples[i - 1] < 0) || (samples[i] < 0 && samples[i - 1] >= 0)) {
				zcrCount++;
			}
		}
	}

	timeRMS = Sqrt(sumSq / N);
	timeZCR = (double)zcrCount / (double)N;
	timeCrestFactor = maxAbs / (timeRMS + 1e-10);
}

void paraInit() {
	// 周波数領域変数の初期化
	frequencyAve = 0;
	frequencyLog = 0;
	spectralFlatness = 0;
	spectralEnergy = 0;
	spectralCentroid = 0;
	spectralSum = 0;
	spectralFlux = 0;
	score = 0;
	spectralPower = 0;

	// 時間領域変数の初期化
	timeRMS = 0;
	timeZCR = 0;
	timeCrestFactor = 0;

	analyzed = false;
}

void paraCalc() {
	// 時間領域の計算を実行
	timeDomainParaCalc(nowSoundBuffer);

	if (recordingSize <= 0) {
		return;
	}

	frequencyAve /= recordingSize * 1.0;
	frequencyLog /= recordingSize;
	frequencyLog = Exp(frequencyLog);
	spectralFlatness = frequencyLog / frequencyAve;
	frequencyAve *= 100.0;

	if (spectralSum == 0) {
		spectralSum += 1e-10;
	}

	spectralCentroid /= spectralSum;

	double temp = 0;

	for (int i = recordingBeginIndent; i < recordingEndIndent; i++) {
		temp += peakBuffer[i];
		if (temp >= spectralSum * 0.85) {
			spectralRolloff = fft.resolution * i;
			break;
		}
	}
}

// シーンマネージャー
using App = SceneManager<String>;

double ConvertVolume(double volume)
{
	return ((volume == 0.0) ? 0.0 : Math::Eerp(0.01, 1.0, volume));
}

// リアルタイムシーン
class RealtimeScene : public App::Scene
{

private:
	// FFTサイズを小さくして、時間的な反応速度を上げる
	Microphone mic{ StartImmediately::Yes };

public:

	//コンストラクタ
	RealtimeScene(const InitData& init)
		: IScene{ init }
	{
		sceneNum = REALTIME_SCENE;
		if (not mic) {
			// マイクを利用できない場合、終了
			throw Error{ U"Microphone not available" };
		}
	}

	// 更新関数
	void update() override
	{
		nowSoundBuffer.clear();
		// FFTの結果を取得
		mic.fft(fft);
		const auto buffer = mic.getBuffer();
		nowSoundBuffer.reserve(buffer.size());

		for (const auto& sample : buffer) {
			nowSoundBuffer.push_back(sample.left);
		}

		ClearPrint();

		// 初回実行時に配列のサイズを合わせる
		softInit();

		// 画面のサイズおよび配列のサイズを取得
		sceneWidth = Scene::Width();
		sceneHeight = Scene::Height();
		bufferSize = (int32)fft.buffer.size();

		// 平均値の初期化
		paraInit();

		for (int32 i = 0; i < bufferSize; ++i)
		{
			// 現在の値
			double currentVal = Pow(fft.buffer[i], 0.4f); // 指数を下げて感度アップ

			displayFrequency(currentVal, i);
		}

		paraCalc();

		// マウス位置の周波数を計算
		mouseUI();

		// UI表示
		buttonUI();

		// テキストボックスUI表示
		textBoxUI();

		if (SimpleGUI::Button(U"Scene:Sound Player", Vec2{ 460, 20 }))
		{
			changeScene(U"SoundPlay");
		}
		if (SimpleGUI::Button(U"Scene:Analyze", Vec2{ 460, 70 }))
		{
			changeScene(U"AnalyzedScene");
		}
	}

	// 描画関数
	void draw() const override
	{

	}
};

// サウンドプレイシーン
class SoundPlay : public App::Scene
{
private:
	// 音楽
	Audio audio;

	// 音量
	double volume = 1.0;

	// 再生位置の変更の有無
	bool seeking = false;

public:

	// コンストラクタ（必ず実装する）
	SoundPlay(const InitData& init)
		: IScene{ init }
	{
		sceneNum = SOUNDPLAY_SCENE;
	}

	~SoundPlay()
	{
		// 終了時に再生中の場合、音量をフェードアウト
		if (audio.isPlaying())
		{
			audio.fadeVolume(0.0, 0.3s);
			System::Sleep(0.3s);
		}
	}

	// 更新関数
	void update() override
	{
		ClearPrint();

		// 再生・演奏時間
		const String time = FormatTime(SecondsF{ audio.posSec() }, U"M:ss")
			+ U" / " + FormatTime(SecondsF{ audio.lengthSec() }, U"M:ss");

		// プログレスバーの進み具合
		double progress = static_cast<double>(audio.posSample()) / audio.samples();

		// 画面サイズの取得
		sceneWidth = Scene::Width();
		sceneHeight = Scene::Height();

		if (audio.isPlaying())
		{
			// FFT 解析
			FFT::Analyze(fft, audio);

			// 初回実行時に配列のサイズを合わせる
			softInit();

			// 配列のサイズを取得
			bufferSize = (int32)fft.buffer.size();

			// 平均値の初期化
			paraInit();

			for (int32 i = 0; i < bufferSize; ++i)
			{
				// 現在の値
				double currentVal = Pow(fft.buffer[i], 0.4f); // 指数を下げて感度アップ

				displayFrequency(currentVal, i);
			}

			paraCalc();

			// マウス位置の周波数を計算
			mouseUI();

			// UI表示
			buttonUI();

			// テキストボックスUI表示
			textBoxUI();

			if (SimpleGUI::Button(U"Scene:Realtime", Vec2{ 460, 20 }))
			{
				changeScene(U"RealtimeScene");
			}
		}

		Rect{ 0, static_cast<int32>(sceneHeight) - 120, static_cast<int32>(sceneWidth), 120 }.draw(ColorF{ 0.5 });

		// フォルダから音楽ファイルを開く
		if (SimpleGUI::Button(U"Open", Vec2{ 40, sceneHeight - 100 }, 100))
		{
			audio.stop(0.5s);
			audio = Dialog::OpenAudio();
			audio.setVolume(ConvertVolume(volume));
			audio.play();
		}

		// 再生
		if (SimpleGUI::Button(U"\U000F040A", Vec2{ 150, sceneHeight - 100 }, 60, (audio && (not audio.isPlaying()))))
		{
			audio.setVolume(ConvertVolume(volume));
			audio.play(0.2s);
		}

		// 一時停止
		if (SimpleGUI::Button(U"\U000F03E4", Vec2{ 220, sceneHeight - 100 }, 60, audio.isPlaying()))
		{
			audio.pause(0.2s);
		}

		// 音量
		if (SimpleGUI::Slider(((volume == 0.0) ? U"\U000F075F" : (volume < 0.5) ? U"\U000F0580" : U"\U000F057E"),
			volume, Vec2{ 40, sceneHeight - 60 }, 30, 120, (not audio.isEmpty())))
		{
			audio.setVolume(ConvertVolume(volume));
		}

		// スライダー
		if (SimpleGUI::Slider(time, progress, Vec2{ 200, sceneHeight - 60 }, 130, 420, (not audio.isEmpty())))
		{
			audio.pause(0.05s);

			while (audio.isPlaying()) // 再生が止まるまで待機
			{
				System::Sleep(0.01s);
			}

			// 再生位置を変更
			audio.seekSamples(static_cast<size_t>(audio.samples() * progress));

			// ノイズを避けるため、スライダーから手を離すまで再生は再開しない
			seeking = true;
		}
		else if (seeking && MouseL.up())
		{
			// 再生を再開
			audio.play(0.05s);
			seeking = false;
		}
	}

	// 描画関数
	void draw() const override
	{

	}
};

// 解析結果表示シーン
class AnalyzedScene : public App::Scene
{

private:
	// FFTサイズを小さくして、時間的な反応速度を上げる
	Microphone mic{ StartImmediately::Yes };

public:

	//コンストラクタ
	AnalyzedScene(const InitData& init)
		: IScene{ init }
	{
		sceneNum = REALTIME_SCENE;
		if (not mic) {
			// マイクを利用できない場合、終了
			throw Error{ U"Microphone not available" };
		}
	}

	// 更新関数
	void update() override
	{
		RectF{ sceneWidth, sceneHeight }.draw(ColorF(time_RF));

		// FFTの結果を取得
		mic.fft(fft);

		nowSoundBuffer.clear();
		const auto buffer = mic.getBuffer();
		nowSoundBuffer.reserve(buffer.size());

		for (const auto& sample : buffer) {
			nowSoundBuffer.push_back(sample.left);
		}


		ClearPrint();

		// 初回実行時に配列のサイズを合わせる
		softInit();

		// 画面のサイズおよび配列のサイズを取得
		sceneWidth = Scene::Width();
		sceneHeight = Scene::Height();
		bufferSize = (int32)fft.buffer.size();

		// 平均値の初期化
		paraInit();

		for (int32 i = 0; i < bufferSize; ++i)
		{
			// 現在の値
			double currentVal = Pow(fft.buffer[i], 0.4f); // 指数を下げて感度アップ

			displayFrequency(currentVal, i);
		}

		paraCalc();

		// 拍手音解析の結果を表示
		analyzed = predictClap();
		if (analyzed) {
			time_RF = 1.1;
		}

		time_RF -= 0.1;

		// マウス位置の周波数を計算
		mouseUI();

		// UI表示
		buttonUI();

		// テキストボックスUI表示
		textBoxUI();

		if (SimpleGUI::Button(U"Scene:Realtime", Vec2{ 460, 20 }))
		{
			changeScene(U"RealtimeScene");
		}
	}
};

void Main()
{
	// Windowサイズを可変にする
	Window::SetStyle(WindowStyle::Sizable);

	// シーンマネージャーを作成
	App manager;

	// タイトルシーンを登録する
	manager.add<RealtimeScene>(U"RealtimeScene");
	manager.add<SoundPlay>(U"SoundPlay");
	manager.add<AnalyzedScene>(U"AnalyzedScene");

	while (System::Update())
	{
		// 現在のシーンを実行する
		// シーンに実装した .update() と .draw() が実行される
		if (not manager.update())
		{
			break;
		}

		// Rキーで録画開始/停止の切り替え
		if (KeyR.down())
		{
			isRecording = !isRecording;

			// 録画開始した時の処理
			if (isRecording) {
				recordingTimeArray.push_back(0);
				rmsArray.push_back(-1);
				zcrArray.push_back(-1);
				crestArray.push_back(-1);
				averageArray.push_back(-1);
				flatnessArray.push_back(-1);
				centroidArray.push_back(-1);
				rolloffArray.push_back(-1);
				fluxArray.push_back(-1);
				powerArray.push_back(-1);
				analyzedArray.push_back(false);

				Array<double> temp;
				for (int i = recordingBeginIndent; i <= recordingEndIndent; i++) {
					temp.push_back(fft.resolution * i);
				}

				waveData.push_back(temp);
			}

			// 録画停止した瞬間に一括保存
			if (!isRecording)
			{
				// 1. CSVオブジェクトを作成
				CSV csv;

				csv.write(U"Num", U"Time", U"RMS", U"ZCR", U"CrestFactor",  U"Power", U"Average", U"Flatness", U"Centroid", U"Roll-off", U"Flux", U"Judge");

				for (size_t i = 0; i < averageArray.size(); i++) {
					csv.newLine();
					csv.write(i, recordingTimeArray[i], rmsArray[i], zcrArray[i], crestArray[i], powerArray[i], averageArray[i], flatnessArray[i], centroidArray[i], rolloffArray[i], fluxArray[i], analyzedArray[i]);

					/*
					for (size_t j = 0; j < waveData[i].size(); j++) {
						csv.write(waveData[i][j]);
					}

					csv.write(U" ");

					for (size_t j = 0; j < soundData[i].size(); j++) {
						csv.write(soundData[i][j]);
					}
					*/
				}

				if (csv.save(U"analysis_data.csv"));

				// 配列の初期化
				recordingTime = 0;
				recordingTimeArray.clear();
				powerArray.clear();
				averageArray.clear();
				flatnessArray.clear();
				centroidArray.clear();
				rolloffArray.clear();
				fluxArray.clear();
				// 追加分
				rmsArray.clear();
				zcrArray.clear();
				crestArray.clear();
				analyzedArray.clear();
				System::MessageBoxOK(U"保存完了");

			}
		}

		if (isRecording)
		{
			recordingTime += Scene::DeltaTime();
			recordingTimeArray.push_back(recordingTime);

			// 特徴量を保存
			powerArray.push_back(spectralPower);
			averageArray.push_back(frequencyAve);
			flatnessArray.push_back(spectralFlatness);
			centroidArray.push_back(spectralCentroid);
			rolloffArray.push_back(spectralRolloff);
			fluxArray.push_back(spectralFlux);

			// 時間領域の新しいパラメータを保存
			rmsArray.push_back(timeRMS);
			zcrArray.push_back(timeZCR);
			crestArray.push_back(timeCrestFactor);

			if (analyzed) {
				analyzedArray.push_back(1);
			}
			else {
				analyzedArray.push_back(0);
			}
			/*
			waveData.push_back(peakBuffer);
			soundData.push_back(nowSoundBuffer);
			*/
		}
	}
}
