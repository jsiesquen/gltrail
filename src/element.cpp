/***************************************************************************
 *   Copyright (C) 2008 by Erlend Simonsen                                 *
 *   mr@fudgie.org                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "element.h"
#include <iostream>
#include <stdlib.h>
#include <math.h>

using namespace std;

Element::Element(Input *h, QString name, QColor col, bool referrer)
{
  x = 1.0 - rand() % 1000 / 500.0;
  y = 1.0 - rand() % 1000 / 500.0;

  vx = 0.0;
  vy = 0.0;

  ax = 0.0000;
  ay = 0.0000;

  size = 1.0;
  realSize = 1.0;
  wantedSize = 1.0;

  messages = 0;
  totalMessages = 0;
  rate = 0.0;

  host = h;
  m_name = name;
  color = col;
  radius = 0.1;

  showInfo = 3;

  external = referrer;

}

Element::~Element()
{
  in.clear();
  out.clear();
  relations_in.clear();
  relations_out.clear();
  activity_queue.clear();
  activities.clear();
}

void Element::add_link_in(Element *e) {
  if( e != NULL ) {
    if(!in.contains( e->name() ) ) {
      in[e->name()] = e;
      relations_in.push_back( new Relation(e,this) );
    }

    for( Relations::iterator it = relations_in.begin(); it != relations_in.end(); ++it ) {
      if( (*it)->getSource() == e ) {
        (*it)->addHit();
      }
    }

    activity_queue << new Activity(e, this);
  }
  messages++;
}

void Element::add_link_out(Element *e) {
  if( e != NULL ) {

    if( !out.contains( e->name() ) ) {
      out[e->name()] = e;
      relations_out.push_back( new Relation(this,e) );
    }

    for( Relations::iterator it = relations_out.begin(); it != relations_out.end(); ++it ) {
      if( (*it)->getTarget() == e ) {
        (*it)->addHit();
      }
    }
  }

  if( external )
    messages++;
}

void Element::update_stats(void) {

  lastSize = wantedSize;

  if( rate == 0.0 && totalMessages == 0 ) {
    rate = messages / 60.0;
  } else {
    rate = (rate * 299.0 + messages) / 300.0;
  }

  if( rate < 0.0001 )
    rate = 0.0;

  totalMessages += messages;
  messages = 0;

  switch( host->getGLWidget()->showSize() ) {
  case 0:
    realSize = rate * 60.0;
    break;
  case 1:
    realSize = relations_in.size();
    break;
  case 2:
    realSize = relations_out.size();
    break;
  case 3:
    realSize = (relations_out.size() + relations_in.size());
    break;
  case 4:
    realSize = totalMessages;
    break;
  }

  wantedSize = realSize;

  float scale = 1.0;
  if( wantedSize > host->getGLWidget()->getMaxSize() ) {
    host->getGLWidget()->setMaxSize( wantedSize );
  } else {
    scale = wantedSize / host->getGLWidget()->getMaxSize();
  }


  if( wantedSize > 5.0 )
    wantedSize = 5.0;

  wantedSize *= scale;

  if( wantedSize < 1.0 )
    wantedSize = 1.0;

  radius = CUTOFF * size / 2;

  if( showInfo > 0 ) {
    showInfo--;
  }

  for(Relations::iterator it = relations_in.begin(); it != relations_in.end(); ++it) {
    (*it)->decayHits();
  }
  for(Relations::iterator it = relations_out.begin(); it != relations_out.end(); ++it) {
    (*it)->decayHits();
  }

}

void Element::update(GLWidget *gl) {

  size = size + (wantedSize-size) / 60.0;

  vx += ax / SMOOTHING;
  vy += ay / SMOOTHING;

  vx *= DAMPENING;
  vy *= DAMPENING;

  x += vx / (size * size);
  y += vy / (size * size);

  ax = 0.0;
  ay = 0.0;

  if( x > 1.0 ) {
    x = 1.0;
    vx = -vx;
    ax = -ax;
  } else if( x < -1.0 ) {
    x = -1.0;
    vx = -vx;
    ax = -ax;
  }

  if( y > gl->getAspect() ) {
    y = gl->getAspect();
    vy = -vy;
    ay = -ay;
  } else if( y < gl->getAspect() * -1.0  ) {
    y = gl->getAspect() * -1.0;
    vy = -vy;
    ay = -ay;
  }

  minX = x - radius;
  maxX = x + radius;
  minY = y - radius;
  maxY = y + radius;

  return;
}

void Element::render(GLWidget *gl) {
   GLfloat r = 0.004 + (size - 1.0) / 100;

   bool hover = fabs(gl->getX() - x) <= r*1.5f && fabs(gl->getY() - y) <= r * 1.5f;

   if( activity_queue.size() > 0 && rand() % (60/activity_queue.size()) == 0 ) {

       Activity *a = activity_queue.takeFirst();
       activities << a;
       if( gl->useRecoil() ) {
         a->fire();
       }
   }

   // Render circle

   if(hover) {
     if( gl->getSelected() == NULL )
       gl->setSelected(this);
     glColor4f(1.0, 1.0, 1.0, 1.0);
   } else {
     if( activities.size() > 0 ) {
       gl->qglColor( host->getColor().lighter( 120) );
     } else {
       gl->qglColor( color );
     }
   }

   if( size == 1.0 ) {
     glTranslatef(x,y,0.0);
     glEnable(GL_LINE_SMOOTH);
     glCallList(gl->circle);
     glDisable(GL_LINE_SMOOTH);
     glTranslatef(-x,-y,0.0);
     gl->stats[STAT_LISTS] += 1;
   } else {
     glEnable(GL_LINE_SMOOTH);
     glBegin(GL_LINE_STRIP);

     GLfloat vy1 = y + r;
     GLfloat vx1 = x;

     for(GLfloat angle = 0.0f; angle <= (2.0f*M_PI); angle += 0.25f) {
       gl->stats[STAT_LINES] += 1;
       glVertex3f(vx1, vy1, 0.0);
       vx1 = x + r * sin(angle);
       vy1 = y + r * cos(angle);
     }
     gl->stats[STAT_LINES] += 2;
     glVertex3f(vx1, vy1, 0.0);
     glVertex3f(x, y+r, 0.0);

     glEnd();
     glDisable(GL_LINE_SMOOTH);
   }

  if( showInfo > 0 || hover ) {
    QString info = QString("[%1] %2").arg( QString::number(realSize).left(5) ).arg(name().left(50));
    glColor4f(1.0, 1.0, 1.0, 1.0);
    int xi =  (int) ((1.0 + x) / 2.0 * gl->getWidth()) - info.length() * 3;
    int xy =  (int) (( gl->getAspect() - y) / (2 * gl->getAspect()) * gl->getHeight() - r - 5.0);

    gl->renderText(xi,xy, info );
  }

  if( activities.size() > 0) {
    glPointSize(3.0);
    glBegin(GL_POINTS);
    for(Activities::iterator it = activities.begin(); it != activities.end(); ++it) {
      if( (*it)->render(gl) ) {
        if( gl->useRecoil() ) {
          (*it)->impact();
        }
        delete *it;
        it = activities.erase(it);
      }
    }
    glEnd();
    glPointSize(1.0);
  }

}

void Element::renderRelations(GLWidget *gl) {
   GLfloat r = 0.004 + (size - 1.0) / 100;

   bool hover = fabs(gl->getX() - x) <= r*1.5f && fabs(gl->getY() - y) <= r * 1.5f;

   // Render relations?
   if( gl->showLines() > 0 || hover ) {

     if( hover && gl->showLines() == 0 ) {
       glEnable(GL_LINE_STIPPLE);
       glEnable(GL_LINE_SMOOTH);
     }
     for(Relations::iterator it = relations_in.begin(); it != relations_in.end(); ++it) {

       if( gl->getMaxHits() < (*it)->getHits() ) {
         gl->setMaxHits( (*it)->getHits() );
       }

       float ratio = (*it)->getHits() / gl->getMaxHits();

       // Ignore if only showing > 10%
       if( gl->showLines() == 2 && ratio < 0.1 )
         continue;

       glLineWidth(1.0 + 2.0 * ratio);
       gl->qglColor( host->getColor().lighter( 10 + (int) (80.0 * ratio)  ) );

       gl->stats[STAT_LINES] += 1;
       glLineStipple(1, gl->stipple_out);

       glBegin(GL_LINES);
       glVertex3f(x,y,0.0);
       glVertex3f((*it)->getSource()->x, (*it)->getSource()->y, 0);
       glEnd();
     }


     if( hover && gl->showLines() == 0 ) {
       for(Relations::iterator it = relations_out.begin(); it != relations_out.end(); ++it) {
         gl->stats[STAT_LINES] += 1;
         glLineStipple(1, gl->stipple_in);

         if( gl->getMaxHits() < (*it)->getHits() ) {
           gl->setMaxHits( (*it)->getHits() );
         }

         float ratio = (*it)->getHits() / gl->getMaxHits();
         glLineWidth(1.0 + 4.0 * ratio);
         gl->qglColor( host->getColor().lighter( 30 + (int) (120.0 * ratio)  ) );

         glBegin(GL_LINES);
         glVertex3f(x,y,0.0);
         glVertex3f((*it)->getTarget()->x, (*it)->getTarget()->y, 0);
         glEnd();
       }
     }

     glLineWidth(1.0);

     if( hover && gl->showLines() == 0 ) {
       glDisable(GL_LINE_SMOOTH);
       glDisable(GL_LINE_STIPPLE);
     }
   }

}

bool Element::contains(GLWidget *gl, Element *e) {
  //  gl->stats["Bounding Box Check"] += 1;
  return( e->x >= minX
          && e->x <= maxX
          && e->y >= minY
          && e->y <= maxY
          );
}

void Element::repulsive_check(GLWidget *gl, Element *e) {
  bool shown = false;
  float dx = (e->x - x) * 5;
  float dy = (e->y - y) * 5;

  float d2 = dx * dx + dy * dy;

  if(d2 < 0.001) {
    dx = rand() % 500 / 1000.0 * 0.05;
    dy = rand() % 500 / 1000.0 * 0.05;
    d2 = dx * dx + dy * dy;
  }

  if(d2 < 0.001) {
    d2 = 0.001;
  }

  double d = sqrt(d2);

  if( d < CUTOFF * size) {
    gl->stats[STAT_REPULSIVE_FORCE] += 1;
    repulsive_force(e, d, dx, dy);

    if( gl->showForces() ) {
      gl->stats[STAT_LINES] += 1;
      glColor3f(0.2, 0.2, 0.2);
      glBegin(GL_LINES);
      glVertex3f(x,y,0.0);
      glVertex3f(e->x, e->y, 0);
      glEnd();
      shown = true;
    }
  }
  if( d < CUTOFF * e->size) {
    gl->stats[STAT_REPULSIVE_FORCE] += 1;
    e->repulsive_force(this, d, -dx, -dy);

    if( gl->showForces() && !shown ) {
      gl->stats[STAT_LINES] += 1;
      glColor3f(0.2, 0.2, 0.2);
      glBegin(GL_LINES);
      glVertex3f(x,y,0.0);
      glVertex3f(e->x, e->y, 0);
      glEnd();
    }
  }
}

void Element::attractive_check(GLWidget *gl, Element *e) {
  float dx = (e->x - x);
  float dy = (e->y - y);

  float d2 = dx * dx + dy * dy;

  if(d2 < 0.001) {
    dx = rand() % 500 / 1000.0 * 0.05;
    dy = rand() % 500 / 1000.0 * 0.05;
    d2 = dx * dx + dy * dy;
  }

  if(d2 < 0.001) {
    d2 = 0.001;
  }

  double d = sqrt(d2);

  if( d > CUTOFF/4  ) {
    gl->stats[STAT_ATTRACTIVE_FORCE] += 1;
    attractive_force(e, d, dx, dy);
  }
}

void Element::repulsive_force(Element *e, double d, float dx, float dy) {
  float ed = (d - CUTOFF * size);
  ed *= log(size) * 0.5 + 1.0;

  float fx = ed / d * dx;
  float fy = ed / d * dy;

  e->ax -= fx;
  e->ay -= fy;

  ax += fx;
  ay += fy;
}

void Element::attractive_force(Element *e, double d, float dx, float dy) {
  float ed = d - ((CUTOFF*size)/4);
  ed *= log(size) * 0.5 + 1.0;

  float fx = dx / d;
  float fy = dy / d;

  fx *= K * ed;
  fy *= K * ed;

  e->ax -= fx;
  e->ay -= fy;

  ax += fx;
  ay += fy;
}
