function rand()
{
    return 4;
}

function elapsedTime(date) {
    var txt = Format.formatDate(date, Formatter.Timepoint)
    var elapsed = Format.formatDate(date, Formatter.DurationElapsed)
    return txt + (elapsed ? ' (' + elapsed + ')' : '')
    //return (elapsed == "Just now") ? '' : elapsed
}
