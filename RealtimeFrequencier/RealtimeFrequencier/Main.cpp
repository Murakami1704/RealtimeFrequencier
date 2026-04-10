# include <Siv3D.hpp>

void Main()
{
	// Windowサイズを可変にする
	Window::SetStyle(WindowStyle::Sizable);

	// FFTサイズを小さくして、時間的な反応速度を上げる
	Microphone mic{ StartImmediately::Yes };
	if (not mic) {
		// マイクを利用できない場合、終了
		throw Error{ U"Microphone not available" };
	}

	FFTResult fft;

	// ピークを保持するための配列
	Array<double> peakBuffer;

	while (System::Update())
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

			// ピーク値を更新（現在の値か、少し減衰させた過去のピーク値の大きい方）
			peakBuffer[i] = Max(currentVal, peakBuffer[i] * 0.92);

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
	}
}
