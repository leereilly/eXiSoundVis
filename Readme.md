[![](http://imgur.com/DWGECXG.gif)](https://www.youtube.com/watch?v=N4eA68BEpak)
<sup>**Note:** The image above is an animated GIF, so there's some quality lost. Please see the [YouTube video](https://www.youtube.com/watch?v=N4eA68BEpak) for higher quality visualization of Disclosure's You and Me.</sup>

This is an Unreal Engine 4 plugin that loads `.ogg` files at runtime and analyzes them to get the frequency spectrum to control gameplay, visualization and more.


Feature list and pictures of available nodes:
---------------------------------------------

* Load `.ogg` file from HardDrive
* Load song names and file paths from HardDrive to display list of songs
* Get frequency spectrum of current loaded song
* Get specific frequency values of the analyzed current song
* (OLD Plugin*) Get the amplitudes of the current loaded song

(OLD Plugin*) This is still the code of the old Plugin. This will not work at it's current state, so make sure to not use it for now! 

![](http://i.imgur.com/mJzto0J.png)


Installation
-------------
Unzip the package into the Plugins directory of your game. 
To add it as an engine plugin you will need to unzip the module into the plugin directory under where you installed UE4.


**1.** Download the [ZIP file](https://github.com/eXifreXi/eXiSoundVis/archive/master.zip).

**2.** Create a `Plugins` folder in your game or engine directory and extract the plugin into it. It should look something like this:

![](http://i.imgur.com/2Y8FvnW.png)

**3.** Open your project (and/or regenerate the Visual Studio files to have the plugin in your solution) and enable it in the plugin section:

![](http://i.imgur.com/Fto3GJT.png)

**4.** Wherever you want to use the plugin, construct a new `UObject` and use the `SoundVisualization` class coming with the plugin:

![](http://i.imgur.com/YVnozvI.png)

**5.** Load a song via its **ABSOLUTE** path (only `.ogg` files):

![](http://i.imgur.com/w82PDun.jpg)

**6.** The loaded song is placed in the `CurrentSoundWave` `USoundWave`* that you can access via the contructed object:

![](http://i.imgur.com/QQbsfFz.png) 

**6.1.** Make sure to only play this if you really loaded the song. Otherwise happy crashing...

**7.** Use the **NEW** Calculate Frequency Spectrum function after you loaded a song to get an Array of Frequency Values to represent the Frequencies from 0 to ~22000hz:

![](http://i.imgur.com/yCktubx.png)

**8.** To loop this, create a timer or use the `GameSeconds` to get the `StartingTime` and loop through it with a simple back going exec wire. Make sure to use a `Delay` (e.g. 0.01) so it won't result in a freeze!

![](http://i.imgur.com/awa4FMc.png)

**9.** Now you can use the different frequency functions to get the values (for example if you want to get the values for bass, use 20 to 60 for SubBass and 60 to 250 for Bass. You can look up more on the internet.

![](http://i.imgur.com/eIzA5Hh.jpg)

**10.** The `SetHeight` function in the above example takes a `MaxRange` float value. This is used to normalize the Frequency Value from 0 to MaxRange to pack it into a 0 to 1.0 range.

This is needed because these values will get **REALLY** big. The smaller the interval, the bigger the numbers!

**11.** You can find all functions available by going to the SoundVis category. They are explained in the [`SoundVisualisation.h`](https://github.com/eXifreXi/eXiSoundVis/blob/master/Source/eXiSoundVis/Classes/SoundVisualization.h) if you don't know how to use them:

![](http://i.imgur.com/dBJqqWG.png)

When cooking, make sure to add the plugin to your projects dependencies!

![](http://i.imgur.com/fh8VB1T.png)


Example project
---------------

Click [here](http://87.106.80.179/Downloads/Tutorial.rar)!


License
-------------
By using this plugin you accept the CC-BY 4.0 license and

The MIT License (MIT)

Copyright (c) 2015 Cedric Neukirchen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


Contact
-------------
If you have any Questions, Comments, Bug reports or feature requests for this plugin, 
or you wish to contact me you can:

email me - cedric.neukirchen@gmx.de

contact me on the forum - Username: eXi

contact me on the Slack Chat - Username: cedric_exi


Credits
--------------
n00854180t 	- Helping a lot to get the loading of the .ogg file running

moss		- Helping to understand wave files
