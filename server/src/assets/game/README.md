# SWTbahn Game Client

This is a simple driver client that the general public can use when the 
SWTbahn is being exhibited.

The main features are:
- Train selection view
  - Pre-defined set of trains shown as infomation cards (photo and specs)
  - Information card of a train has a button to grab it if it is available
  - Availability of a Train is updated every 0.5s (train is on the tracks 
    and has not been grabbed)
  - Grabbing a train will bring the user to the driving view
  - When multiple users grab the same train, only one user will succeed
  - Refreshing or closing the game client in this view has no ill consequences 
    on the global game state
- Driving view
  - Pre-defined set of possible destinations (`destinations-swtbahn-*.js`) 
    from the train's current position
  - Availability of a destination (route) is updated every 0.5s (route is not 
    granted, route has no conflicts, and route is clear) 
  - Choosing an available destination hides the set of possible destinations,
    shows the chosen destination, and shows the possible train speed
    (stop, slow, normal, fast)
  - When multiple users choose the same destination, only one user will succeed
  - Users must confirm that they have stopped before their destination signal
    before the next set of possible destinations will be shown
  - Confirmation is only possible when the train is on the destination's 
    main segment (checked every 0.5s)
  - Trains are automatically stopped when they overshoot their destination signal
  - User can end their game at any time, except when a user has started driving 
    their train and have not yet reached their chosen destination
  - When a user ends their game, their train and granted route are released and 
    they are brought back to the train selection view
  - When a user refreshes or closes the game client, their train is automatically
    stopped and their train and granted route are released
- General
  - Positive and negative feedback messages and sounds are used to communicate
    good and bad driving behaviours to the user

The main limitations are:
  - Users cannot cancel their chosen destination and choose another one
  - Train must be on an ordinary block (straight track flanked by signals) in 
    order to compute its next set of possible destinations
  - Trains stopped entirely over a point or over a block that is not flanked by 
    signals will not have any possible next destinations
  - When a user refreshes or closes the game client while driving their train, 
    the train may be forced to stop over a piece of track that is not part of an
    ordinary block and, thus, will not have any next destinations when the train 
    is grabbed again
  - When a user's web browser puts the game client to sleep (e.g., user has 
    switched to another tab, or user has become inactive) or the user closes
    the game client through a tab manager, the game client is unable to take
    any action (e.g., to release their train and granted route). There is no
    reliable method of distinguishing between users who have abandoned their 
    game from those who are looking for their destination or talking to others
    about their game play. 
    See [Page Lifecycle API](https://developer.chrome.com/blog/page-lifecycle-api/#developer-recommendations-for-each-state) 
  - Users typically can no longer play their game when their device loses 
    (Wi-Fi) connection to the server, and their train has to be released
    manually
  - Game state is not restored when the game client is reloaded or reopened


## Source File Organisation

The source files of the SWTbahn Game client are organised in a model-view-controller-like
manner:

- Model and Controller
  - `destinations-swtbahn-full.js`
  - `destinations-swtbahn-standard.js`
  - `destinations-swtbahn-ultraloop.js`
  - `flags-swtbahn-full.js`
  - `script-game.js`
- View
  - `driver-game.html`
  - `style-game.css`

### Model and Controller
The destinations possible from an ordinary block are stored as a JSON lookup table in 
the `destinations-swtbahn-*.js` files, specific to each SWTbahn platform. The flags 
associated with each destination are stored as a JSON lookup table in the `flags-swtbahn-*.js`
files. These tables are used in `script-game.js`.

The game logic and behaviour are defined in `script-game.js` and is organised 
roughly into the following responsibilities:
- Helper functions to hide and show different user interface elements
  - Destination buttons
  - Speed buttons
  - Destination reached button
  - Alert box with speech feedback
  - Modal dialog with speech feedback
- Helper functions to update the possible destinations from a given block and to update their availability
- Driver class for train and driving related behaviour
  - Get the train's current block
  - Get the train availability
  - Grab a train
  - Releasing a train
  - Get the required physical driving direction of a given route based on the train's current orientation
  - Set the grabbed train's speed
  - Automatically show the confirmation of reaching the driver's chosen destination
  - Request a specific route ID to be granted
  - Prepare and track the manual driving of a granted route
  - Release a granted route
- Start game logic
- End game logic
- Initialise game logic
- Handlers when the game client is refreshed or closeed

JavaScript uses a single-threaded event-loop to execute function calls.
A function is always executed to completion, without any interruption.
Thus, busy-waiting statements, e.g., `flag = true; while (flag) { ... }`, will block indefinitely
because `flag` cannot be changed to `false` by another function. When 
communicating with a server, a synchronous request would block the JavaScript
runtime until a response is received. Server communication can be 
made less wasteful by using asynchronous requests or functions. When the 
JavaScript runtime reaches an asynchronous function, it places the function on a 
[microtask queue](https://developer.mozilla.org/en-US/docs/Web/API/HTML_DOM_API/Microtask_guide)
and will execute it at a later time (when its execution stack is empty).

[AJAX](https://api.jquery.com/jquery.ajax/) 
is a common approach to asynchronous server communication, which returns a promise
that needs to be fulfilled before any further action can be taken. Waiting
for a promise to be fulfilled is easily achieved with a 
[promise chain](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Using_promises).
Note that, when the server returns an error, the promise becomes rejected and 
the remainder of the promise chain is skipped. A rejected promise behaves as an
error that needs to be caught and handled.

A pitfall with asynchronous communication is that execution dependencies between
asynchronous calls are not guaranteed to be respected. For example, if the user 
is presented with multiple buttons that trigger some server operation and the operations
should be executed one at a time, mutual exclusion of the asynchronous communication is needed.

> When reviewing the code, always remember that functions execute to completion and
> all of its contained promises (and asynchronous code) will only be executed at a later time.
> Functions that return a promise are named with the suffix `Promise`.

> To keep the functions that communicate with the server reusable, 
> their success and error handlers should not modify any aspect of the user interface. 
> Interface elements should instead be modified by the caller.

### View
The structural elements of the game client are defined in `driver-game.html` and
is styled, themed, and laid out using [Bootstrap](https://getbootstrap.com).
Additional styling that cannot be accomplished alone by Boostrap is supplemented 
by `style-game.css`, e.g., the train details, the alert box fade animation, and the
destination flags.

