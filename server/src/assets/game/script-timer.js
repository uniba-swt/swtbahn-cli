var sec = 0;
var stop = 1;
function add(){
    if(stop == 0){
        sec++;
        document.getElementById("timer").innerHTML = seconds;
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
}

