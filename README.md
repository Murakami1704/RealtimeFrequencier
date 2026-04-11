# RealtimeFrequencier
リアルタイムで周波数を表示するソフトウェア

こちらを参考にして作りました。  
https://siv3d.github.io/ja-jp/samples/sound/  
  
使用する際は、Releaseよりexeファイルをダウンロードし、起動してください。  
マイク入力がないとエラーで起動できません。  
  
変更履歴  
v1.0 リリース  
  
v1.1 軽量化  
シンプルにリアルタイムのフーリエ解析結果を見たい人はこれをご利用ください。  
  
v1.2 モード選択機能の追加  
「通常」「残る」を追加。  
「通常」...素直に現在の音声の周波数特性を表示します。  
「残る」...音量が急激に減少した時などに、波形が徐々に落ち着く(残像)を追加するモードです。  
微妙な差があるので、試してみてください。    
    
    
イメージ図(v1.1時点)↓   
<img width="691" height="369" alt="Image" src="https://github.com/user-attachments/assets/f1262328-10aa-4abf-bbb3-dcc38e6dcfc5" />  

  
開発環境  
OpenSiv3D v0.6.16  
Visual Studio 2026 18.4.3  
