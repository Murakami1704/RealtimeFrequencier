# include <Siv3D.hpp>

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

	FFTResult fft;

	// ピークを保持するための配列
	Array<double> peakBuffer;

	// 定数定義
	const int NORMAL_MODE = 1;
	const int REMAIN_MODE = 2;

	// 変数定義
	int mode = NORMAL_MODE;

public:

	//コンストラクタ
	RealtimeScene(const InitData& init)
		: IScene{ init }
	{
		if (not mic) {
			// マイクを利用できない場合、終了
			throw Error{ U"Microphone not available" };
		}
	}

	// 更新関数
	void update() override
	{
		// FFTの結果を取得
		mic.fft(fft);

		ClearPrint();

		// 初回実行時に配列のサイズを合わせる
		if (peakBuffer.size() != fft.buffer.size())
		{
			peakBuffer.resize(fft.buffer.size(), 0.0);
		}

		// 画面のサイズおよび配列のサイズを取得
		const double sceneWidth = Scene::Width();
		const double sceneHeight = Scene::Height();
		const int32 bufferSize = (int32)fft.buffer.size();

		for (int32 i = 0; i < bufferSize; ++i)
		{
			// 現在の値
			double currentVal = Pow(fft.buffer[i], 0.4f); // 指数を下げて感度アップ

			// ピーク値の表示
			if (mode == NORMAL_MODE) {
				// 現在の値を表示
				peakBuffer[i] = currentVal;
			}
			else {
				// 現在の値か、少し減衰させた過去のピーク値の大きい方
				peakBuffer[i] = Max(currentVal, peakBuffer[i] * 0.92);
			}
			double x = (double)i / bufferSize * sceneWidth;
			double h = peakBuffer[i] * 800; // 倍率を調整

			// 描画
			RectF{ Arg::bottomLeft(x, sceneHeight), 2, h }.draw(HSV{ 240 - i * 0.05, 0.8, 1.0 });
		}

		// マウス位置の周波数を計算
		double mouseXIdx = (Cursor::Pos().x / sceneWidth) * bufferSize;
		double frequency = mouseXIdx * fft.resolution;

		Rect{ Cursor::Pos().x, 0, 1, Scene::Height() }.draw(Palette::White);

		Print << U"周波数: {:.1f} Hz (idx: {})"_fmt(frequency, (int32)mouseXIdx);
		if (frequency > 1000) {
			Print << U"({:.2f} kHz)"_fmt(frequency / 1000.0);
		}

		// UI表示
		if (SimpleGUI::Button(U"Mode:Normal", Vec2{ 260, 20 }))
		{
			mode = NORMAL_MODE;
		}

		if (SimpleGUI::Button(U"Mode:Remain", Vec2{ 260, 70 }))
		{
			mode = REMAIN_MODE;
		}

		if (SimpleGUI::Button(U"Scene:Sound Player", Vec2{ 460, 20 }))
		{
			changeScene(U"SoundPlay");
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

	// FFT の結果
	FFTResult fft;

	// ピークを保持するための配列
	Array<double> peakBuffer;

	// 再生位置の変更の有無
	bool seeking = false;

	// 定数定義
	const int NORMAL_MODE = 1;
	const int REMAIN_MODE = 2;

	// 変数定義
	int mode = NORMAL_MODE;

public:

	// コンストラクタ（必ず実装する）
	SoundPlay(const InitData& init)
		: IScene{ init }
	{

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
		const double sceneWidth = Scene::Width();
		const double sceneHeight = Scene::Height();

		if (audio.isPlaying())
		{
			// FFT 解析
			FFT::Analyze(fft, audio);

			// 初回実行時に配列のサイズを合わせる
			if (peakBuffer.size() != fft.buffer.size())
			{
				peakBuffer.resize(fft.buffer.size(), 0.0);
			}

			// 配列のサイズを取得
			const int32 bufferSize = (int32)fft.buffer.size();

			for (int32 i = 0; i < bufferSize; ++i)
			{
				// 現在の値
				double currentVal = Pow(fft.buffer[i], 0.4f); // 指数を下げて感度アップ

				// ピーク値の表示
				if (mode == NORMAL_MODE) {
					// 現在の値を表示
					peakBuffer[i] = currentVal;
				}
				else {
					// 現在の値か、少し減衰させた過去のピーク値の大きい方
					peakBuffer[i] = Max(currentVal, peakBuffer[i] * 0.92);
				}
				double x = (double)i / bufferSize * sceneWidth;
				double h = peakBuffer[i] * 800; // 倍率を調整

				// 描画
				RectF{ Arg::bottomLeft(x, sceneHeight - 120), 2, h }.draw(HSV{ 240 - i * 0.05, 0.8, 1.0 });
			}

			// マウス位置の周波数を計算
			double mouseXIdx = (Cursor::Pos().x / sceneWidth) * bufferSize;
			double frequency = mouseXIdx * fft.resolution;

			Rect{ Cursor::Pos().x, 0, 1, Scene::Height() }.draw(Palette::White);

			Print << U"周波数: {:.1f} Hz (idx: {})"_fmt(frequency, (int32)mouseXIdx);
			if (frequency > 1000) {
				Print << U"({:.2f} kHz)"_fmt(frequency / 1000.0);
			}

			// UI表示
			if (SimpleGUI::Button(U"Mode:Normal", Vec2{ 260, 20 }))
			{
				mode = NORMAL_MODE;
			}

			if (SimpleGUI::Button(U"Mode:Remain", Vec2{ 260, 70 }))
			{
				mode = REMAIN_MODE;
			}

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

void Main()
{
	// Windowサイズを可変にする
	Window::SetStyle(WindowStyle::Sizable);

	// シーンマネージャーを作成
	App manager;

	// タイトルシーンを登録する
	manager.add<RealtimeScene>(U"RealtimeScene");
	manager.add<SoundPlay>(U"SoundPlay");

	while (System::Update())
	{
		// 現在のシーンを実行する
		// シーンに実装した .update() と .draw() が実行される
		if (not manager.update())
		{
			break;
		}
	}
}
