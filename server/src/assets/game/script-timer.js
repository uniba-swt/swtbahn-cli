class Timer{
    sec = 0;
    min = 0;
    stop = 1;

    timerInvervall;
    tick(){
        if(stop == 0){
            this.sec++;
            if (this.sec == 60){
                this.sec = 0;
                this.min++;
            }
            _min = ('0' + this.min.toString()).substr( -2 );
            _sec = ('0' + this.sec.toString()).substr( -2 );
            $("#timerSecond").html(_sec);
            $("#timerMinute").html(_min);
        }
    }
    start(){
        this.timerInvervall = setInterval(tick(), 1000);
    }
    stop(){
        clearInterval(this.timerInvervall);
        this.sec = 0;
        this.min = 0;
    }
}