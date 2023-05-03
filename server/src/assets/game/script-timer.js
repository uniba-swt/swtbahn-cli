var sec = 0;
var min = 0;
var stop = 1;
function add(){
    if(stop == 0){
        sec++;
        if (sec == 60){
            sec = 0;
            min++;
        }
        _min = ('0' + min.toString()).substr( -2 );
        _sec = ('0' + sec.toString()).substr( -2 );
        $("#timerSecond").html(_sec);
        $("#timerMinute").html(_min);
        timer();
    }
}
function timer(){
    t = setTimeout(add, 1000);
}
function starttimer(){
    stop = 0;
    timer();
}
function stoptimer(){
    stop = 1;
    sec = 0;
    min = 0;
}
