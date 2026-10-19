      // ---- MotherVR v2 pages -------------------------------------------
      // Spliced into frontend.GameMenu of NEWFRONTENDMENU, PAUSEMENU and
      // GAMEOVERMENU by scripts/ui_pages.py. One sub-page per settings
      // section of mothervr.json (Comfort, Controls, Hands, Body,
      // Interaction, HUD, Graphics; spec 22 §12, 21 §15.3, 23 §8, 24 §10),
      // with the spec's exact labels, values and description lines. A
      // control sends "mvr_<name> <value>" through ExternalInterface.call
      // directly (spec 06 §4.1); the DLL pushes each
      // value through the public setMother* functions below, which never
      // send a command back (spec 06 §4.3).

      protected var _motherV2Running:Boolean = false;

      protected var _motherV2Height:Number = 0;

      protected var _motherV2ScaleSwitch:Number = 0;

      // The page's own countdown after Calibrate Standing Height: 3, 2, 1,
      // then -1 (waiting) until the next push; 0 shows the pushed state.
      protected var _motherV2Countdown:int = 0;

      protected var _motherV2CalState:int = 0;

      protected var _motherV2CalSeconds:int = 0;

      protected var _motherComfortItem:MenuItemExpandable;

      protected var _motherControlsItem:MenuItemExpandable;

      protected var _motherHandsItem:MenuItemExpandable;

      protected var _motherBodyItem:MenuItemExpandable;

      protected var _motherInteractionItem:MenuItemExpandable;

      protected var _motherHudItem:MenuItemExpandable;

      protected var _motherGraphicsItem:MenuItemExpandable;

      protected var _motherCalibrateItem:MenuItem;

      protected var _motherV2Items:Object;

      protected var _motherV2Keys:Dictionary;

      protected var _motherV2Commands:Dictionary;

      protected var _motherV2Timer:Timer;

      // Called at the end of setVisibility: after every stock page and item
      // exists, so the new ones take later automatic ids (spec 14 §3.1), and
      // before the subclass assigns menu.rootPage, so they are in its maps.
      protected function motherV2Build() : void
      {
         var page:MenuPage = null;
         this._motherV2Items = new Object();
         this._motherV2Keys = new Dictionary();
         this._motherV2Commands = new Dictionary();
         this._motherComfortItem = new MenuItemExpandable("Comfort");
         this._motherControlsItem = new MenuItemExpandable("Controls");
         this._motherHandsItem = new MenuItemExpandable("Hands");
         this._motherBodyItem = new MenuItemExpandable("Body");
         this._motherInteractionItem = new MenuItemExpandable("Interaction");
         this._motherHudItem = new MenuItemExpandable("HUD");
         this._motherGraphicsItem = new MenuItemExpandable("Graphics");
         // After Input, before Dev. Debug Options (spec 22 §12.1).
         this._motherOptionsItem.page.addItemAtIndex(this._motherComfortItem,3);
         this._motherOptionsItem.page.addItemAtIndex(this._motherControlsItem,4);
         this._motherOptionsItem.page.addItemAtIndex(this._motherHandsItem,5);
         this._motherOptionsItem.page.addItemAtIndex(this._motherBodyItem,6);
         this._motherOptionsItem.page.addItemAtIndex(this._motherInteractionItem,7);
         this._motherOptionsItem.page.addItemAtIndex(this._motherHudItem,8);
         this._motherOptionsItem.page.addItemAtIndex(this._motherGraphicsItem,9);

         page = this._motherComfortItem.page;
         this.motherV2List(page,"SmoothTurnSpeed","Smooth Turn Speed","mvr_smoothturnspeed",this.motherV2Steps(0,15,24,1,30),this.motherV2Labels(0,15,24,1,30,"","°/s"),"Needs Snap Rotation off. Controller Sensitivity applies only when this is Off");
         this.motherV2List(page,"SnapTurnRepeatMs","Snap Rotation Repeat","mvr_snapturnrepeat",this.motherV2Steps(0,50,19,1,150),this.motherV2Labels(0,50,19,1,150,""," ms"),"How often a held stick repeats a snap turn. Off: one turn per push");
         this.motherV2Toggle(page,"SnapTurnFade","Snap Rotation Fade","mvr_snapturnfade","Hides each snap turn behind a short fade from black");
         this.motherV2List(page,"Vignette","Comfort Vignette","mvr_vignette",this.motherV2Steps(0,1,11,10,0),this.motherV2Labels(0,10,11,1,0,"","%"),"Darkens the edge of the view while you move or turn with the stick");
         this.motherV2List(page,"PlayMode","Play Mode","mvr_playmode",[0,1],["Seated","Standing"],"Standing: your real height sets the view height. Applies at the next recenter");
         this._motherCalibrateItem = new MenuItem("Calibrate Standing Height",this.motherV2CalibratePressHandler);
         page.addItem(this._motherCalibrateItem);
         this.motherV2Slider(page,"WorldScale","World Scale","mvr_worldscale",this.motherV2Steps(50,5,21,100,0),"Above 1 the world looks smaller, below 1 larger");

         page = this._motherControlsItem.page;
         this.motherV2Toggle(page,"InteractionAwareTurning","No Turning During Interactions","mvr_interactionawareturning","The stick does not turn you at keypads, terminals, levers and other interactions");
         this.motherV2Toggle(page,"DpadEmulation","D-Pad Emulation","mvr_dpademulation","The left stick is the D-pad in menus and interactions, or while you hold its click");
         this.motherV2Toggle(page,"RecenterGesture","Recenter Gesture","mvr_recentergesture","Hold both grips with both hands at your face to recenter");
         this.motherV2Toggle(page,"HudToggleGesture","HUD Toggle Gesture","mvr_hudtogglegesture","Left hand at your head and Map Menu hides or shows the HUD, prompts included");
         this.motherV2Toggle(page,"HolsterGestures","Holster Gestures","mvr_holstergestures","Grip over a shoulder: weapons. Left hip: flashlight. Chest: motion tracker");
         this.motherV2Toggle(page,"HidingLean","Lean While Hiding","mvr_hidinglean","Not available yet");
         this.motherV2Toggle(page,"HoldBreathGesture","Hold Breath Gesture","mvr_holdbreathgesture","Not available yet");
         this.motherV2Toggle(page,"ControllerGlyphs","Per-Controller Glyphs","mvr_controllerglyphs","Button prompts show the buttons of the controllers you are holding");
         this.motherV2Toggle(page,"VrOffHotkey","VR Off Hotkey","mvr_vroffhotkey","Hold Right Ctrl + Right Shift + F10 for 1 second to turn VR off until restart");
         this.motherV2Toggle(page,"CrashSafeLog","Crash-Safe Log","mvr_crashsafelog","Requires restart! Writes the log to disk at once, so it survives a system crash");

         page = this._motherHandsItem.page;
         this.motherV2Toggle(page,"HandsEnabled","Tracked Hands","mvr_hands","Your tracked hands are used in the game, as set below");
         this.motherV2Toggle(page,"AimFromHand","Aim from Hand","mvr_aimfromhand","The weapon aims where your hand points, not where you look");
         this.motherV2Toggle(page,"TrackerInHand","Motion Tracker in Hand","mvr_trackerinhand","Hold up the motion tracker in your left hand and look at it to read it");
         this.motherV2List(page,"FlashlightAim","Flashlight Aim","mvr_flashlightaim",[0,1,2],["Head","Left Hand","Right Hand"],"Not available yet");
         this.motherV2Toggle(page,"MeleeByMotion","Melee by Swing","mvr_meleebymotion","Swing your hand to strike with a melee weapon");
         this.motherV2Toggle(page,"ThrowByMotion","Throw by Motion","mvr_throwbymotion","Thrown items fly with your hand\'s motion");
         this.motherV2Slider(page,"ThrowStrength","Throw Strength","mvr_throwstrength",this.motherV2Steps(5,1,16,10,0),"Multiplies the speed of your throws");
         this.motherV2Slider(page,"HandSmoothing","Hand Smoothing","mvr_handsmoothing",this.motherV2Steps(0,1,11,10,0),"Steadies the hands. Higher values steady more but lag behind");
         this.motherV2Toggle(page,"WristHealth","Wrist Health","mvr_wristhealth","Turn your left wrist up to see your health");
         this.motherV2Toggle(page,"HapticsEnabled","Haptics","mvr_haptics","Requires restart! Controllers vibrate for game events");
         this.motherV2Slider(page,"HapticsStrength","Haptics Strength","mvr_hapticsstrength",this.motherV2Steps(0,1,11,10,0),"Strength of every vibration");
         this.motherV2Toggle(page,"HapticsGameRumble","Vibrate on Game Rumble","mvr_hapticsgamerumble","Vibrate whenever the game would rumble a gamepad");
         this.motherV2Toggle(page,"HapticsDamage","Vibrate on Damage","mvr_hapticsdamage","Vibrate when you are hurt");
         this.motherV2Toggle(page,"HapticsTracker","Vibrate on Tracker Pings","mvr_hapticstracker","A faint tick for each motion tracker ping");
         this.motherV2Toggle(page,"HapticsFiring","Vibrate on Firing","mvr_hapticsfiring","Vibrate your right hand when you fire");
         this.motherV2Toggle(page,"HapticsHacking","Vibrate on Hacking","mvr_hapticshacking","Vibrate when hacking or cutting starts and ends");

         page = this._motherBodyItem.page;
         this.motherV2List(page,"BodyVisible","Body","mvr_bodyvisible",[0,1,2,3],["Full","Arms and Hands","Hands Only","None"],"Which parts of your character\'s body you see");
         this.motherV2Slider(page,"EyeForward","Eye Forward","mvr_eyeforward",this.motherV2Steps(-20,1,41,100,0),"Moves your view forward (positive) or back from the body, in metres");
         this.motherV2Toggle(page,"RoomScale","Room-Scale","mvr_roomscale","Walking in your play space moves you in the game");
         this.motherV2Slider(page,"SeatedEyeRaise","Seated Eye Raise","mvr_seatedeyeraise",this.motherV2Steps(-50,5,21,100,0),"Raises (positive) or lowers the view when seated, in metres");
         this.motherV2Toggle(page,"RealCrouch","Real Crouch","mvr_realcrouch","Crouch for real to crouch in the game. Needs Standing play and a calibration");

         // Spec 23 §8. Levers by Hand and the two distances have no effect in
         // this build (23 §12), so they are shown greyed; Camera Takeovers is
         // not (r8: Reduced applies 23 §4.2 (b) and (c)).
         page = this._motherInteractionItem.page;
         this.motherV2Toggle(page,"PhysicalLevers","Levers by Hand","mvr_physicallevers","Not available yet");
         this.motherV2List(page,"CameraTakeovers","Camera Takeovers","mvr_cameratakeovers",[0,1],["Game","Reduced"],"Reduced: fewer interactions take the view away from your head");
         this.motherV2List(page,"TerminalDistance","Terminal Distance","mvr_terminaldistance",this.motherV2Steps(0,1,16,10,6),this.motherV2Tenths(this.motherV2Steps(0,1,16,10,6)," m"),"Not available yet");
         this.motherV2List(page,"RewireDistance","Rewire Distance","mvr_rewiredistance",this.motherV2Steps(0,1,16,10,6),this.motherV2Tenths(this.motherV2Steps(0,1,16,10,6)," m"),"Not available yet");
         this.motherV2List(page,"TerminalTextScale","Terminal Text Size","mvr_terminaltextscale",[1,1.25,1.5,1.75],["100%","125%","150%","175%"],"Requires restart! Larger text on terminal screens");

         page = this._motherHudItem.page;
         this.motherV2List(page,"Crosshair","Crosshair","mvr_crosshair",[0,1,2],["On","Off","Auto"],"Auto: hidden while the weapon aims from your hand");
         this.motherV2Toggle(page,"HudHidden","Hide HUD","mvr_hidehud","Starts with the HUD hidden, prompts and objectives included");
         this.motherV2Toggle(page,"ItemHighlight","Item Highlight","mvr_itemhighlight","Glow on items you can pick up. A change may apply only from the next level");
         this.motherV2List(page,"HudPlacement","HUD Placement","mvr_hudplacement",[0,1],["Head-Locked","Follow"],"Follow: the HUD stays level and catches up when you turn");
         this.motherV2Slider(page,"HudDistance","HUD Distance","mvr_huddistance",this.motherV2Steps(6,1,15,10,0),"Distance of the HUD from your eyes, in metres");
         this.motherV2Slider(page,"HudWidth","HUD Width","mvr_hudwidth",this.motherV2Steps(30,5,19,100,0),"Width in metres. A smaller HUD also makes its text smaller");
         this.motherV2Slider(page,"HudPitch","HUD Pitch","mvr_hudpitch",this.motherV2Steps(-30,5,9,1,0),"Moves the HUD below (negative) or above your view, in degrees");

         // Spec 24 §10: the items of the features in this build (5 to 9,
         // 11 to 13); the others are not shown.
         page = this._motherGraphicsItem.page;
         this.motherV2Toggle(page,"EngineSettingsOverride","Engine Settings Override","mvr_enginesettingsoverride","Requires restart! Adds the VR options below to the game\'s graphics settings");
         this.motherV2Toggle(page,"ShadowMap4096","4096 Shadow Maps","mvr_shadowmap4096","Requires restart! Adds 4096 to Shadow Map Resolution in the Video options");
         this.motherV2Toggle(page,"SharpReflections","Sharp Reflections","mvr_sharpreflections","Requires restart! Adds a Planar Reflections option without blur");
         this.motherV2List(page,"StreamingTextureHeapMB","Texture Streaming Memory","mvr_streamingtextureheap",[0,300,400,500,600,700,800,900,1000,1024],["Game","300 MB","400 MB","500 MB","600 MB","700 MB","800 MB","900 MB","1000 MB","1024 MB"],"Requires restart! Memory for streaming textures. Game: the game\'s own size");
         this.motherV2List(page,"StreamingModelHeapMB","Model Streaming Memory","mvr_streamingmodelheap",[0,128,192,256,320,384,448,512],["Game","128 MB","192 MB","256 MB","320 MB","384 MB","448 MB","512 MB"],"Requires restart! Memory for streaming models. Game: the game\'s own size");
         this.motherV2Toggle(page,"LensFlares","Lens Flares in VR","mvr_lensflares","Off: no lens flares in the headset");
         this.motherV2Toggle(page,"ChromaticAberration","Chromatic Aberration in VR","mvr_chromaticaberration","Off: no color fringes in the headset, whatever the Video setting");
         this.motherV2Toggle(page,"FilmGrain","Film Grain in VR","mvr_filmgrain","Off: no film grain in the headset, whatever the Video setting");

         this.motherV2UpdateState();
      }

      // "0.6 m", "1.0 m": one decimal and `suffix`, "Off" for 0 (spec 23
      // §8: a list value is read as a measure, so it keeps its unit).
      protected function motherV2Tenths(values:Array, suffix:String) : Array
      {
         var labels:Array = new Array();
         var tenths:int = 0;
         var i:int = 0;
         while(i < values.length)
         {
            tenths = int(Math.round(values[i] * 10));
            labels.push(tenths == 0 ? "Off" : String(int(tenths / 10)) + "." + String(tenths % 10) + suffix);
            i++;
         }
         return labels;
      }

      // `count` values first/den, (first + step)/den, ...; with `offFirst`
      // above 0 the first value is 0 and the rest are offFirst/den,
      // (offFirst + step)/den, ... (a list whose first entry is Off).
      protected function motherV2Steps(first:int, step:int, count:int, den:int, offFirst:int) : Array
      {
         var values:Array = new Array();
         var i:int = 0;
         while(i < count)
         {
            if(offFirst > 0)
            {
               values.push(i == 0 ? 0 : (offFirst + (i - 1) * step) / den);
            }
            else
            {
               values.push((first + i * step) / den);
            }
            i++;
         }
         return values;
      }

      // The labels of motherV2Steps: "Off" for a leading 0, else the
      // number with `prefix` and `suffix`.
      protected function motherV2Labels(first:int, step:int, count:int, den:int, offFirst:int, prefix:String, suffix:String) : Array
      {
         var labels:Array = new Array();
         var values:Array = this.motherV2Steps(first,step,count,den,offFirst);
         var i:int = 0;
         while(i < count)
         {
            if(values[i] == 0)
            {
               labels.push("Off");
            }
            else
            {
               labels.push(prefix + String(values[i]) + suffix);
            }
            i++;
         }
         return labels;
      }

      protected function motherV2Register(page:MenuPage, item:MenuItem, key:String, command:String, description:String) : void
      {
         item.description = description;
         page.addItem(item);
         this._motherV2Items[key] = item;
         this._motherV2Keys[item] = key;
         this._motherV2Commands[item] = command;
      }

      protected function motherV2Toggle(page:MenuPage, key:String, label:String, command:String, description:String) : void
      {
         this.motherV2Register(page,new MenuItemOptionCheckBox(label,true,null,this.motherV2IndexChangeHandler),key,command,description);
      }

      protected function motherV2List(page:MenuPage, key:String, label:String, command:String, values:Array, labels:Array, description:String) : void
      {
         this.motherV2Register(page,new MenuItemOptionOptionStepper(label,values,labels,this.motherV2IndexChangeHandler),key,command,description);
      }

      // A slider through fixed steps: the number beside the bar is the
      // step's own label, a plain decimal (spec 22 §12.1).
      protected function motherV2Slider(page:MenuPage, key:String, label:String, command:String, values:Array, description:String) : void
      {
         var labels:Array = new Array();
         var i:int = 0;
         var item:MenuItemOptionSlider = new MenuItemOptionSlider(label,values,this.motherV2IndexChangeHandler,true,0,10,true);
         while(i < values.length)
         {
            labels.push(String(values[i]));
            i++;
         }
         item.labels = labels;
         this.motherV2Register(page,item,key,command,description);
      }

      // The player changed a control: send its value.
      protected function motherV2IndexChangeHandler(param1:MenuItemEvent) : void
      {
         var item:* = param1.item;
         var command:String = this._motherV2Commands[item];
         if(command == null)
         {
            return;
         }
         if(item is MenuItemOptionCheckBox)
         {
            ExternalInterface.call(command + " " + int(item.selectedItem));
         }
         else
         {
            ExternalInterface.call(command + " " + String(item.selectedItem));
         }
         this.motherV2UpdateState();
      }

      protected function motherV2CalibratePressHandler(param1:MenuItemEvent) : void
      {
         if(!this._motherCalibrateItem.enabled)
         {
            return;
         }
         ExternalInterface.call("mvr_calibrateheight");
         // The DLL waits as long before it measures (spec 22 §6.2).
         this._motherV2Countdown = 3;
         this.motherV2UpdateState();
         if(this._motherV2Timer == null)
         {
            this._motherV2Timer = new Timer(1000,0);
            this._motherV2Timer.addEventListener(TimerEvent.TIMER,this.motherV2TimerHandler,false,0,true);
         }
         this._motherV2Timer.reset();
         this._motherV2Timer.start();
      }

      protected function motherV2TimerHandler(param1:TimerEvent) : void
      {
         this._motherV2Countdown--;
         if(this._motherV2Countdown <= 0)
         {
            // The result reaches the page with the next push (the menu's
            // next opening); until then the page cannot tell whether the
            // measurement was taken.
            this._motherV2Timer.stop();
            this._motherV2Countdown = -1;
         }
         this.motherV2UpdateState();
      }

      // The calibrate button's description line (spec 22 §12.2).
      protected function motherV2CalibrationText() : String
      {
         var state:int = this._motherV2CalState;
         var seconds:int = this._motherV2CalSeconds;
         if(this._motherV2Countdown != 0)
         {
            state = this._motherV2Countdown > 0 ? 1 : 2;
            seconds = this._motherV2Countdown;
         }
         if(state == 1)
         {
            return "Measuring in " + Math.max(1,seconds);
         }
         if(state == 2)
         {
            return "Waiting for head tracking";
         }
         if(state == 4)
         {
            return "Not measured: height outside 1.00 to 2.20 m";
         }
         if(state == 5)
         {
            return "Not measured: no floor height on this runtime";
         }
         if(this._motherV2Height > 0)
         {
            return "Standing height: " + this.motherV2TwoDecimals(this._motherV2Height) + " m. Select to measure again";
         }
         return "Stand up straight, select, and keep still";
      }

      protected function motherV2Item(key:String) : *
      {
         return this._motherV2Items == null ? null : this._motherV2Items[key];
      }

      protected function motherV2On(key:String) : Boolean
      {
         var item:* = this.motherV2Item(key);
         return item != null && Boolean(item.selectedItem);
      }

      protected function motherV2Enable(key:String, enabled:Boolean) : void
      {
         var item:* = this.motherV2Item(key);
         if(item != null)
         {
            item.enabled = enabled;
         }
      }

      // "1.72": two decimals.
      protected function motherV2TwoDecimals(value:Number) : String
      {
         var hundredths:int = int(Math.round(value * 100));
         var cents:int = hundredths % 100;
         return String(int(hundredths / 100)) + "." + (cents < 10 ? "0" : "") + String(cents);
      }

      // Greying (spec 22 §12.1, §12.2, 21 §15.3); holds whatever order the
      // pushes arrive in.
      protected function motherV2UpdateState() : void
      {
         var tracked:Boolean = false;
         var haptics:Boolean = false;
         var standing:Boolean = false;
         var engine:Boolean = false;
         var worldScale:* = null;
         if(this._motherV2Items == null)
         {
            return;
         }
         this._motherComfortItem.enabled = this._motherV2Running;
         this._motherControlsItem.enabled = this._motherV2Running;
         this._motherHandsItem.enabled = this._motherV2Running;
         this._motherBodyItem.enabled = this._motherV2Running;
         this._motherInteractionItem.enabled = this._motherV2Running;
         this._motherHudItem.enabled = this._motherV2Running;
         this._motherGraphicsItem.enabled = this._motherV2Running;
         tracked = this.motherV2On("HandsEnabled");
         this.motherV2Enable("AimFromHand",tracked);
         this.motherV2Enable("TrackerInHand",tracked);
         this.motherV2Enable("MeleeByMotion",tracked);
         this.motherV2Enable("ThrowByMotion",tracked);
         this.motherV2Enable("ThrowStrength",tracked && this.motherV2On("ThrowByMotion"));
         this.motherV2Enable("HandSmoothing",tracked);
         // Shown greyed until the feature is confirmed in game.
         this.motherV2Enable("FlashlightAim",false);
         this.motherV2Enable("HidingLean",false);
         this.motherV2Enable("HoldBreathGesture",false);
         this.motherV2Enable("PhysicalLevers",false);
         this.motherV2Enable("TerminalDistance",false);
         this.motherV2Enable("RewireDistance",false);
         engine = this.motherV2On("EngineSettingsOverride");
         this.motherV2Enable("ShadowMap4096",engine);
         this.motherV2Enable("SharpReflections",engine);
         this.motherV2Enable("StreamingTextureHeapMB",engine);
         this.motherV2Enable("StreamingModelHeapMB",engine);
         haptics = this.motherV2On("HapticsEnabled");
         this.motherV2Enable("HapticsStrength",haptics);
         this.motherV2Enable("HapticsGameRumble",haptics);
         this.motherV2Enable("HapticsDamage",haptics);
         this.motherV2Enable("HapticsTracker",haptics);
         this.motherV2Enable("HapticsFiring",haptics);
         this.motherV2Enable("HapticsHacking",haptics);
         standing = this.motherV2Item("PlayMode").selectedItem == 1;
         this.motherV2Enable("SeatedEyeRaise",!standing);
         this.motherV2Enable("RealCrouch",standing && this._motherV2Height > 0);
         worldScale = this.motherV2Item("WorldScale");
         if(this._motherV2ScaleSwitch > 0)
         {
            worldScale.enabled = false;
            worldScale.selectedIndex = this.motherV2Nearest(worldScale.items,this._motherV2ScaleSwitch);
            worldScale.description = "Set by -vrscale on the command line for this session";
         }
         this._motherCalibrateItem.description = this.motherV2CalibrationText();
      }

      // The index of the value in `values` nearest `value` (spec 22 §12.1:
      // a stored value between steps is shown as the nearest one).
      protected function motherV2Nearest(values:Array, value:Number) : int
      {
         var best:int = 0;
         var i:int = 1;
         while(i < values.length)
         {
            if(Math.abs(values[i] - value) < Math.abs(values[best] - value))
            {
               best = i;
            }
            i++;
         }
         return best;
      }

      // Sets a control from a pushed value; the item's setters do not fire
      // its handler (spec 06 §7.2), so nothing is sent back.
      protected function motherV2Set(key:String, value:*) : void
      {
         var item:* = this.motherV2Item(key);
         if(item == null)
         {
            return;
         }
         if(item is MenuItemOptionCheckBox)
         {
            item.selectedItem = Boolean(value);
         }
         else
         {
            item.selectedIndex = this.motherV2Nearest(item.items,Number(value));
         }
         this.motherV2UpdateState();
      }

      protected function motherV2SetRunning(param1:Boolean) : void
      {
         this._motherV2Running = param1;
         this.motherV2UpdateState();
      }

      public function setMotherSmoothTurnSpeed(param1:Number) : void
      {
         this.motherV2Set("SmoothTurnSpeed",param1);
      }

      public function setMotherSnapTurnRepeatMs(param1:Number) : void
      {
         this.motherV2Set("SnapTurnRepeatMs",param1);
      }

      public function setMotherSnapTurnFade(param1:Boolean) : void
      {
         this.motherV2Set("SnapTurnFade",param1);
      }

      public function setMotherVignette(param1:Number) : void
      {
         this.motherV2Set("Vignette",param1);
      }

      public function setMotherPlayMode(param1:Number) : void
      {
         this.motherV2Set("PlayMode",param1);
      }

      public function setMotherStandingEyeHeight(param1:Number) : void
      {
         this._motherV2Height = param1;
         this.motherV2UpdateState();
      }

      // The DLL's calibration state (CalibrationPageState): it replaces the
      // page's own countdown.
      public function setMotherCalibrationState(param1:int) : void
      {
         this._motherV2CalState = param1;
         this._motherV2Countdown = 0;
         if(this._motherV2Timer != null)
         {
            this._motherV2Timer.stop();
         }
         this.motherV2UpdateState();
      }

      public function setMotherCalibrationSeconds(param1:int) : void
      {
         this._motherV2CalSeconds = param1;
         this.motherV2UpdateState();
      }

      public function setMotherWorldScale(param1:Number) : void
      {
         this.motherV2Set("WorldScale",param1);
      }

      // The -vrscale scale of this session, 0 without the switch.
      public function setMotherWorldScaleSwitch(param1:Number) : void
      {
         this._motherV2ScaleSwitch = param1;
         this.motherV2UpdateState();
      }

      public function setMotherInteractionAwareTurning(param1:Boolean) : void
      {
         this.motherV2Set("InteractionAwareTurning",param1);
      }

      public function setMotherDpadEmulation(param1:Boolean) : void
      {
         this.motherV2Set("DpadEmulation",param1);
      }

      public function setMotherRecenterGesture(param1:Boolean) : void
      {
         this.motherV2Set("RecenterGesture",param1);
      }

      public function setMotherHudToggleGesture(param1:Boolean) : void
      {
         this.motherV2Set("HudToggleGesture",param1);
      }

      public function setMotherHolsterGestures(param1:Boolean) : void
      {
         this.motherV2Set("HolsterGestures",param1);
      }

      public function setMotherHidingLean(param1:Boolean) : void
      {
         this.motherV2Set("HidingLean",param1);
      }

      public function setMotherHoldBreathGesture(param1:Boolean) : void
      {
         this.motherV2Set("HoldBreathGesture",param1);
      }

      public function setMotherControllerGlyphs(param1:Boolean) : void
      {
         this.motherV2Set("ControllerGlyphs",param1);
      }

      public function setMotherVrOffHotkey(param1:Boolean) : void
      {
         this.motherV2Set("VrOffHotkey",param1);
      }

      public function setMotherCrashSafeLog(param1:Boolean) : void
      {
         this.motherV2Set("CrashSafeLog",param1);
      }

      public function setMotherHandsEnabled(param1:Boolean) : void
      {
         this.motherV2Set("HandsEnabled",param1);
      }

      public function setMotherAimFromHand(param1:Boolean) : void
      {
         this.motherV2Set("AimFromHand",param1);
      }

      public function setMotherTrackerInHand(param1:Boolean) : void
      {
         this.motherV2Set("TrackerInHand",param1);
      }

      public function setMotherFlashlightAim(param1:Number) : void
      {
         this.motherV2Set("FlashlightAim",param1);
      }

      public function setMotherMeleeByMotion(param1:Boolean) : void
      {
         this.motherV2Set("MeleeByMotion",param1);
      }

      public function setMotherThrowByMotion(param1:Boolean) : void
      {
         this.motherV2Set("ThrowByMotion",param1);
      }

      public function setMotherThrowStrength(param1:Number) : void
      {
         this.motherV2Set("ThrowStrength",param1);
      }

      public function setMotherHandSmoothing(param1:Number) : void
      {
         this.motherV2Set("HandSmoothing",param1);
      }

      public function setMotherWristHealth(param1:Boolean) : void
      {
         this.motherV2Set("WristHealth",param1);
      }

      public function setMotherHapticsEnabled(param1:Boolean) : void
      {
         this.motherV2Set("HapticsEnabled",param1);
      }

      public function setMotherHapticsStrength(param1:Number) : void
      {
         this.motherV2Set("HapticsStrength",param1);
      }

      public function setMotherHapticsGameRumble(param1:Boolean) : void
      {
         this.motherV2Set("HapticsGameRumble",param1);
      }

      public function setMotherHapticsDamage(param1:Boolean) : void
      {
         this.motherV2Set("HapticsDamage",param1);
      }

      public function setMotherHapticsTracker(param1:Boolean) : void
      {
         this.motherV2Set("HapticsTracker",param1);
      }

      public function setMotherHapticsFiring(param1:Boolean) : void
      {
         this.motherV2Set("HapticsFiring",param1);
      }

      public function setMotherHapticsHacking(param1:Boolean) : void
      {
         this.motherV2Set("HapticsHacking",param1);
      }

      public function setMotherBodyVisible(param1:Number) : void
      {
         this.motherV2Set("BodyVisible",param1);
      }

      public function setMotherEyeForward(param1:Number) : void
      {
         this.motherV2Set("EyeForward",param1);
      }

      public function setMotherRoomScale(param1:Boolean) : void
      {
         this.motherV2Set("RoomScale",param1);
      }

      public function setMotherSeatedEyeRaise(param1:Number) : void
      {
         this.motherV2Set("SeatedEyeRaise",param1);
      }

      public function setMotherRealCrouch(param1:Boolean) : void
      {
         this.motherV2Set("RealCrouch",param1);
      }

      public function setMotherPhysicalLevers(param1:Boolean) : void
      {
         this.motherV2Set("PhysicalLevers",param1);
      }

      public function setMotherCameraTakeovers(param1:Number) : void
      {
         this.motherV2Set("CameraTakeovers",param1);
      }

      public function setMotherTerminalDistance(param1:Number) : void
      {
         this.motherV2Set("TerminalDistance",param1);
      }

      public function setMotherRewireDistance(param1:Number) : void
      {
         this.motherV2Set("RewireDistance",param1);
      }

      public function setMotherTerminalTextScale(param1:Number) : void
      {
         this.motherV2Set("TerminalTextScale",param1);
      }

      public function setMotherCrosshair(param1:Number) : void
      {
         this.motherV2Set("Crosshair",param1);
      }

      public function setMotherHudHidden(param1:Boolean) : void
      {
         this.motherV2Set("HudHidden",param1);
      }

      public function setMotherItemHighlight(param1:Boolean) : void
      {
         this.motherV2Set("ItemHighlight",param1);
      }

      public function setMotherHudPlacement(param1:Number) : void
      {
         this.motherV2Set("HudPlacement",param1);
      }

      public function setMotherHudDistance(param1:Number) : void
      {
         this.motherV2Set("HudDistance",param1);
      }

      public function setMotherHudWidth(param1:Number) : void
      {
         this.motherV2Set("HudWidth",param1);
      }

      public function setMotherHudPitch(param1:Number) : void
      {
         this.motherV2Set("HudPitch",param1);
      }

      public function setMotherEngineSettingsOverride(param1:Boolean) : void
      {
         this.motherV2Set("EngineSettingsOverride",param1);
      }

      public function setMotherShadowMap4096(param1:Boolean) : void
      {
         this.motherV2Set("ShadowMap4096",param1);
      }

      public function setMotherSharpReflections(param1:Boolean) : void
      {
         this.motherV2Set("SharpReflections",param1);
      }

      public function setMotherStreamingTextureHeapMB(param1:Number) : void
      {
         this.motherV2Set("StreamingTextureHeapMB",param1);
      }

      public function setMotherStreamingModelHeapMB(param1:Number) : void
      {
         this.motherV2Set("StreamingModelHeapMB",param1);
      }

      public function setMotherLensFlares(param1:Boolean) : void
      {
         this.motherV2Set("LensFlares",param1);
      }

      public function setMotherChromaticAberration(param1:Boolean) : void
      {
         this.motherV2Set("ChromaticAberration",param1);
      }

      public function setMotherFilmGrain(param1:Boolean) : void
      {
         this.motherV2Set("FilmGrain",param1);
      }
